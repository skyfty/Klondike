
// KlondikeDlg.h : header file
//

#pragma once


// CKlondikeDlg dialog
class CKlondikeDlg : public CDialogEx
{
// Construction
public:
	CKlondikeDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_KLONDIKE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
private:
	CEdit m_Number;
public:
	CProgressCtrl m_Progress;
	afx_msg void OnBnClickedOk();
};
