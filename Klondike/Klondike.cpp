// Klondike.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "framework.h"
#include <iostream>
#include <shlwapi.h>
#include <CommCtrl.h>
#include <filesystem>
#include <map>
#include <boost/process.hpp>
#include <boost/program_options/detail/convert.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/process/windows.hpp>
#include "Klondike.h"
#include "sqlite_orm.h"
#include "solvitaire.h"

// Global Variables:
HINSTANCE hInst;                                // current instance
#define WM_SOLVE_COMPLETE (WM_USER + 1000)
#define WM_SOLVE_SETP (WM_USER + 1001)

INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    MainWndProc(HWND, UINT, WPARAM, LPARAM);

char module_path[1024] = { 0 };
char rule_file_path[1024] = { 0, };
char database_file_path[1024] = { 0, };

using namespace sqlite_orm;


void AllocLogConsole() {
    ::AllocConsole();
    freopen("conin$", "r", stdin);
    freopen("conout$", "w", stdout);
    freopen("conout$", "w", stderr);
}

HWND MainWnd = NULL;
HWND ProgressWnd = NULL;
HWND ListWnd = NULL;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    InitCommonControls();
    AllocLogConsole();

    GetModuleFileNameA(NULL, module_path, sizeof(module_path));
    PathRemoveFileSpecA(module_path);
    PathCombineA(rule_file_path, module_path, "rule.json");
    PathCombineA(database_file_path, module_path, "game.db");

    DialogBox(hInst, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, MainWndProc);
    return EXIT_SUCCESS;
}

struct GameDeal {
    std::string game;
    uint64_t moves;
    std::chrono::milliseconds time;

};

auto MakeStorage(const char* file_path) {
    auto storage = make_storage(file_path,
        make_table("deal",
            make_column("game", &GameDeal::game),
            make_column("moves", &GameDeal::moves)));
    storage.sync_schema();
    return storage;
}
typedef decltype(MakeStorage(nullptr)) DbStorage;
std::unique_ptr<DbStorage> storage;

bool is_terminate_solve = false;
void terminate_solve(bool b) {
    is_terminate_solve = b;
    sigint_handler(b);
}

bool on_solve_callback(solve_state state, solve_state* out_state) {
    *out_state = state;
    return true;
}

void on_gen_game_deal(const char* buffer, size_t number, solve_state *state) {
    std::string game_buffer(buffer, number);
    auto game_deals = storage->get_all<GameDeal>(where((c(&GameDeal::game) == game_buffer)));
    if (game_deals.size()) {
        return;
    }

    solve_buffer(rule_file_path, buffer, number, 604800, &on_solve_callback, state);
    if (state->sol_type == solve_state::type::SOLVED) {
        GameDeal gd;
        gd.game = game_buffer;
        gd.moves = state->dominance_moves;
        gd.time = state->time;
        storage->insert(gd);
        SendMessage(MainWnd, WM_SOLVE_SETP, 0, (LPARAM)&gd);
    }
}

void LunchWorkThread(int number) {
    while (!is_terminate_solve && number) {
        solve_state state;
        gen_game_deal(GetTickCount64(), rule_file_path, &on_gen_game_deal, &state);

        if (state.sol_type == solve_state::type::SOLVED) {
            number--;
        }
    }
    PostMessage(MainWnd, WM_SOLVE_COMPLETE, 0, 0);
}

std::unique_ptr<std::thread> work_thread;

INT_PTR CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SOLVE_SETP: {
        GameDeal *gd = (GameDeal*)lParam;
        
        LVITEMW lvI = { 0 };
        lvI.mask = LVIF_TEXT | LVIF_PARAM;// | LVIF_NORECOMPUTE;// | LVIF_IMAGE;
        lvI.iItem = 0;
        lvI.iSubItem = 0;
        lvI.lParam = 0;
        std::wstring game = boost::from_utf8(gd->game);
        lvI.pszText = (LPWSTR)game.c_str();
        lvI.iItem = ListView_InsertItem(ListWnd, &lvI);
        {
            std::wstring steps = boost::str(boost::wformat(L"%1%") % gd->moves);
            
            LPWSTR steps_text = (LPWSTR)steps.c_str();
            ListView_SetItemText(ListWnd, lvI.iItem, 1, steps_text);
        }
        {
            std::wstring steps;
            auto time_count = gd->time.count();
            if (time_count < 1000) {
                steps = boost::str(boost::wformat(L"%1%毫秒") % time_count);
            }
            else if(time_count < 1000 * 60) {
                time_count = std::chrono::duration_cast<std::chrono::seconds>(gd->time).count();
                steps = boost::str(boost::wformat(L"%1%秒") % time_count);
            } else {
                time_count = std::chrono::duration_cast<std::chrono::minutes>(gd->time).count();
                steps = boost::str(boost::wformat(L"%1%分") % time_count);
            }
            LPWSTR steps_text = (LPWSTR)steps.c_str();
            ListView_SetItemText(ListWnd, lvI.iItem, 2, steps_text);
        }
   
        ListView_EnsureVisible(ListWnd, lvI.iItem, FALSE);

        SendMessageA(ProgressWnd, PBM_STEPIT, 0, 0);
        break;
    }
    case WM_INITDIALOG: {
        MainWnd = hWnd;
        RECT rect;
        GetWindowRect(hWnd, &rect);
        rect.left = (GetSystemMetrics(SM_CXSCREEN) - rect.right) / 2;
        rect.top = (GetSystemMetrics(SM_CYSCREEN) - rect.bottom) / 2;
        SetWindowPos(hWnd, HWND_TOP, rect.left, rect.top, rect.right, rect.bottom, SWP_SHOWWINDOW);

        ProgressWnd = GetDlgItem(hWnd, IDC_PROGRESS);
        SendMessage(ProgressWnd, PBM_SETSTEP, 1, 0);

        ListWnd = GetDlgItem(hWnd, IDC_LIST);
        LVCOLUMN lvc;
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = 0;
        lvc.pszText = (LPWSTR)L"牌局";
        lvc.cx = 310;  
        ListView_InsertColumn(ListWnd, 0, &lvc);

        lvc.iSubItem = 1;
        lvc.pszText = (LPWSTR)L"步数";
        lvc.cx = 50;
        ListView_InsertColumn(ListWnd, 1, &lvc);

        lvc.iSubItem = 2;
        lvc.pszText = (LPWSTR)L"耗时";
        lvc.cx = 50;
        ListView_InsertColumn(ListWnd, 2, &lvc);

        storage.reset(new DbStorage(MakeStorage(database_file_path)));

        break;
    }
    case WM_SOLVE_COMPLETE: {
        terminate_solve(false);
        work_thread.release();
        SetDlgItemText(hWnd, IDOK, L"开始");
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK ) {
            if (work_thread) {
                terminate_solve(true);
                work_thread->join();
                PostMessage(hWnd, WM_SOLVE_COMPLETE, 0, 0);
            }
            else {
                int number = GetDlgItemInt(hWnd, IDC_NUMBER, NULL, FALSE);
                if (number > 0) {
                    SendMessage(ProgressWnd, PBM_SETRANGE, 0, MAKELPARAM(0, number));
                    SendMessage(ProgressWnd, PBM_SETPOS, 0, 0);
                    work_thread.reset(new std::thread(std::bind(&LunchWorkThread, number)));
                    SetDlgItemText(hWnd, IDOK, L"停止");
                }
            }
            return (INT_PTR)TRUE;
        }
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    }
    return (INT_PTR)FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_DESTROY:
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


