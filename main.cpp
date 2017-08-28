#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <Commdlg.h >
#include <shobjidl.h> //For IShellItemImageFactory
//#include <afxwin.h>

#include "library.h"

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include "Resource.h"

HINSTANCE		g_hInst;
HWND			g_hWnd;
POINT			g_OriginalSize;
POINT			g_PreviousSize;
HANDLE			g_Mutex;
Library*		g_Library = NULL;
Documents		g_CurrentSelection;
std::wstring	g_CurrentFilter;
Document*		g_CurrentDocument = NULL;
HWND			g_FilterEditBox;
HWND			g_ListBox;
HWND			g_KeywordEditBox;
HWND			g_StatusBar;
int				g_SortIndex = 0;
bool			g_SortAscending = true;
std::wstring	g_AddDialogPath;
HANDLE			g_hCFGChange;


BOOL                InitInstance(HINSTANCE, int);
INT_PTR CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK		AddDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
void				PopulateListbox(const DocumentList& documents);
void				ProcessFilterEditBox();
void				SortList();
void				Rescan();
void				CopyToClipboard();
void				UpdateThumbnail();

//---------------------------------------------------------------------------------------------------
// wWinMain
//---------------------------------------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	{
		TCHAR savePathBuffer[512];
		GetModuleFileName(NULL, savePathBuffer, 512);
		
		std::wstring savePath = savePathBuffer;
		std::wstring::size_type p = savePath.find_last_of(L"\\/");

		if (p != std::wstring::npos)
		{
			savePath = savePath.substr(0, p);
		}

		g_Library = new Library(savePath);

		g_Library->LoadMeta();

		PopulateListbox(g_Library->GetDocuments());

		g_Library->SaveMeta();

		// ?????
		savePath = L"\\?\\" + g_Library->GetSaveFilename();
		g_hCFGChange = FindFirstChangeNotification(savePath.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
		
		if (g_hCFGChange != INVALID_HANDLE_VALUE)
		{
			SetTimer(g_hWnd, 2, 2000, NULL);
		}
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (msg.message == WM_KEYDOWN)
		{
			if (msg.wParam == VK_F5)
			{
				Rescan();
			}
			else if (msg.wParam == VK_ESCAPE)
			{
				DestroyWindow(g_hWnd);
			}
			else if (msg.wParam == VK_DOWN && GetFocus() == g_FilterEditBox)
			{
				msg.wParam = VK_TAB;
			}
			else if (msg.wParam == 'C' && GetKeyState(VK_CONTROL) && GetFocus() == g_ListBox)
			{
				CopyToClipboard();
			}
			else if (msg.wParam == VK_ADD && GetKeyState(VK_CONTROL))
			{
				SendMessage(g_hWnd, WM_COMMAND, IDC_BUTTON_ADD, NULL);
				continue;
			}
		}

		if (!IsDialogMessage(g_hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

//---------------------------------------------------------------------------------------------------
// InitInstance
//---------------------------------------------------------------------------------------------------
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	g_hInst = hInstance;

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	g_Mutex = CreateMutex(NULL, FALSE, L"VPGLibrarian__892389237874568");

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		HWND existingInstance = FindWindow(0, L"VGP Librarian");
		if (existingInstance) SetForegroundWindow(existingInstance);
		exit(0);
	}

	g_hWnd = CreateDialog(	g_hInst,
							MAKEINTRESOURCE(IDD_DIALOG_MAIN),
							NULL,
							WndProc);

	RECT rect;
	GetClientRect(g_hWnd, &rect);
	g_OriginalSize.x = rect.right - rect.left;
	g_OriginalSize.y = rect.bottom - rect.top;
	g_PreviousSize = g_OriginalSize;

	HICON hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_SMALL));

	SendMessage(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

	g_StatusBar = CreateWindowEx(	0,
									STATUSCLASSNAME,
									L"Hello",
									SBARS_SIZEGRIP | WS_CHILD | WS_VISIBLE,
									0, 0, 0, 0,
									g_hWnd,
									(HMENU)IDC_STATUSBAR,
									g_hInst,
									NULL);

	DragAcceptFiles(g_hWnd, TRUE);

	g_FilterEditBox = GetDlgItem(g_hWnd, IDC_EDIT_FILTER);
	g_ListBox = GetDlgItem(g_hWnd, IDC_LIST);
	g_KeywordEditBox = GetDlgItem(g_hWnd, IDC_EDIT_KEYWORDS);

	// Setup list view
	{
		ListView_SetExtendedListViewStyle(g_ListBox, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		
		LVCOLUMN lvColumn;
		memset(&lvColumn, 0, sizeof(lvColumn));
		lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

		lvColumn.pszText = L"Filename";
		lvColumn.fmt = LVCFMT_RIGHT;
		ListView_InsertColumn(g_ListBox, 0, &lvColumn);

		lvColumn.pszText = L"Authors";
		lvColumn.fmt = LVCFMT_LEFT;
		ListView_InsertColumn(g_ListBox, 1, &lvColumn);

		lvColumn.pszText = L"Company";
		lvColumn.fmt = LVCFMT_LEFT;
		ListView_InsertColumn(g_ListBox, 2, &lvColumn);
	}

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	return TRUE;
}

//---------------------------------------------------------------------------------------------------
// WndProc
//---------------------------------------------------------------------------------------------------
INT_PTR CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
		case WM_DESTROY:
			if (g_Library)
				g_Library->SaveMeta();
			PostQuitMessage(0);
			break;

		case WM_CLOSE:
			DestroyWindow(g_hWnd);
			break;

		case WM_COMMAND:
		{
			if (HIWORD(wParam) == EN_CHANGE && lParam == (LPARAM)g_FilterEditBox)
			{
				SetTimer(g_hWnd, 1, 100, NULL);
			}

			if (HIWORD(wParam) == EN_CHANGE && lParam == (LPARAM)g_KeywordEditBox)
			{
				if (g_CurrentDocument)
				{
					TCHAR buffer[1024];
					GetWindowText(g_KeywordEditBox, buffer, 1024);

					std::wstring keywords = buffer;

					g_CurrentDocument->SetKeywords(buffer);

					//Document* doc = g_CurrentDocument;
					//g_CurrentDocument = NULL;
					//SetWindowText(g_KeywordEditBox, doc->m_keywords.c_str());
					//SetFocus(g_KeywordEditBox);
					//SendMessage(g_KeywordEditBox, EM_SETSEL, 0xffff, 0xffff);
					//g_CurrentDocument = doc;
				}
			}

			if (wParam == IDOK)
			{
				if (g_CurrentDocument)
				{
					std::wstring::size_type p = g_CurrentDocument->m_path.find_last_of(L"\\/");
					std::wstring path = g_CurrentDocument->m_path.substr(0, p);

					ShellExecute(NULL, L"open", g_CurrentDocument->m_path.c_str(), NULL, path.c_str(), SW_MAXIMIZE);
					DestroyWindow(g_hWnd);
				}
			}

			if (wParam == ID_OPEN_FOLDER)
			{
				if (g_CurrentDocument)
				{
					std::wstring::size_type p = g_CurrentDocument->m_path.find_last_of(L"\\/");
					std::wstring path = g_CurrentDocument->m_path.substr(0, p);
					
					TCHAR buffer[256];
					swprintf(buffer, 256, L"/select,%s", g_CurrentDocument->m_path.c_str());

					ShellExecute(NULL, L"open", L"explorer.exe", buffer, NULL, SW_MAXIMIZE);
				}
			}

			if (wParam == IDC_BUTTON_HELP)
			{
				MessageBox(g_hWnd,
					L"A lightweight library & annotation app.\n\n"\
					L"- Drag & Drop files & folders to add to library\n"\
					L"- F5 to rescan\n"\
					L"- Double click or Enter to open\n"\
					L"- Right-click for context menu\n",
					L"VGP Librarian",
					MB_OK);
			}

			if (wParam == IDC_BUTTON_ADD)
			{
				g_AddDialogPath = L"";
				if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_ADD), g_hWnd, (DLGPROC)AddDlgProc) == IDOK)
				{
					g_CurrentDocument = g_Library->AddDocument(g_AddDialogPath, L"", true);
					g_Library->SaveMeta();
					g_CurrentSelection.Clear();
					g_CurrentFilter = L"";

					ProcessFilterEditBox();

					SetFocus(g_KeywordEditBox);
				}
			}

			break;
		}

		case WM_TIMER:
		{
			if (wParam == 1)
			{
				ProcessFilterEditBox();
				KillTimer(g_hWnd, 1);
			}
			else if (wParam == 2)
			{
				if (WaitForSingleObject(g_hCFGChange, 0) == WAIT_OBJECT_0)
				{
					if (MessageBox(g_hWnd, L"CFG modified externally. Reload?", L"Reload CFG", MB_OK) == IDOK)
					{
						g_Library->LoadMeta();
						g_Library->SaveMeta();

						g_CurrentSelection = g_Library->Filter(L"");
						g_CurrentFilter = L"";
						ProcessFilterEditBox();
					}
				}
			}
			break;
		}

		case WM_NOTIFY:
		{
			if (((LPNMHDR)lParam)->code == LVN_ITEMCHANGED && ((LPNMHDR)lParam)->hwndFrom == g_ListBox)
			{
				int index = ListView_GetNextItem(g_ListBox, -1, LVNI_SELECTED);

				if (index >= 0)
				{
					LVITEM lvItem;
					memset(&lvItem, 0, sizeof(lvItem));
					lvItem.mask = LVIF_PARAM;
					lvItem.iItem = index;
					lvItem.iSubItem = 0;

					ListView_GetItem(g_ListBox, &lvItem);

					Document* doc = (Document*)lvItem.lParam;

					g_CurrentDocument = NULL;

					SetWindowText(g_KeywordEditBox, doc->m_keywords.c_str());

					g_CurrentDocument = doc;

					UpdateThumbnail();
				}
			}

			if (((LPNMHDR)lParam)->code == LVN_COLUMNCLICK && ((LPNMHDR)lParam)->hwndFrom == g_ListBox)
			{
				int columnIndex = ((NMLISTVIEW*)(lParam))->iSubItem;

				if (columnIndex == g_SortIndex)
				{
					g_SortAscending = !g_SortAscending;
				}
				g_SortIndex = columnIndex;

				SortList();
			}
			
			if (((LPNMHDR)lParam)->code == NM_DBLCLK && ((LPNMHDR)lParam)->hwndFrom == g_ListBox)
			{
				//if (g_CurrentDocument)
				//{
				//	ShellExecute(NULL, L"open", g_CurrentDocument->m_path.c_str(), NULL, NULL, SW_MAXIMIZE);
				//}
				SendMessage(g_hWnd, WM_COMMAND, IDOK, NULL);
			}

			if (((LPNMHDR)lParam)->code == NM_RCLICK && ((LPNMHDR)lParam)->hwndFrom == g_ListBox)
			{
				if (g_CurrentDocument)
				{
					HMENU hMenu = CreatePopupMenu();
					InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_OPEN_FOLDER, L"Open Containing Folder");
					POINT p;
					GetCursorPos(&p);
					TrackPopupMenu(hMenu, TPM_LEFTALIGN, p.x, p.y, 0, g_hWnd, NULL);
				}
			}

			break;
		}

		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;

			int fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

			for (int i=0; i!=fileCount; ++i)
			{
				TCHAR buffer[256];
				DragQueryFile(hDrop, i, buffer, 256);

				g_CurrentDocument = g_Library->AddDocument(buffer, L"", true);
			}

			g_Library->SaveMeta();
			ProcessFilterEditBox();
			SetFocus(g_KeywordEditBox);

			DragFinish(hDrop);

			break;
		}

		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
			lpMMI->ptMinTrackSize.x = 500;
			lpMMI->ptMinTrackSize.y = 400;
			break;
		}

		case WM_SIZE:
		{
			// Ughh......it's 2017! Why am I doing this???
			if (wParam != SIZE_MINIMIZED && hWnd == g_hWnd)
			{
				int width = LOWORD(lParam);
				int height = HIWORD(lParam);

				POINT delta = {width - g_PreviousSize.x, height - g_PreviousSize.y};

				RECT rect;

				int rightAligned[] = {IDC_BUTTON_ADD, IDC_BUTTON_HELP, IDC_THUMBNAIL};
				int bottomAligned[] = {IDC_BUTTON_HELP, IDC_STATUSBAR, IDC_EDIT_KEYWORDS};
				int resizeWidth[] = {IDC_LIST, IDC_EDIT_FILTER, IDC_EDIT_KEYWORDS, IDC_STATUSBAR};
				int resizeHeight[] = {IDC_LIST};

				for (auto ID : resizeWidth)
				{
					HWND hwnd = GetDlgItem(g_hWnd, ID);

					GetWindowRect(hwnd, &rect);
					MapWindowPoints(HWND_DESKTOP, g_hWnd, (LPPOINT)&rect, 2);
				
					rect.right += delta.x;

					::SetWindowPos(		hwnd, HWND_TOP, 
										rect.left, rect.top, 
										rect.right - rect.left, rect.bottom - rect.top, 
										SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS);
				}

				for (auto ID : resizeHeight)
				{
					HWND hwnd = GetDlgItem(g_hWnd, ID);

					GetWindowRect(hwnd, &rect);
					MapWindowPoints(HWND_DESKTOP, g_hWnd, (LPPOINT)&rect, 2);
				
					rect.bottom += delta.y;

					::SetWindowPos(		hwnd, HWND_TOP, 
										rect.left, rect.top, 
										rect.right - rect.left, rect.bottom - rect.top, 
										SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS);
				}

				for (auto ID : rightAligned)
				{
					HWND hwnd = GetDlgItem(g_hWnd, ID);

					GetWindowRect(hwnd, &rect);
					MapWindowPoints(HWND_DESKTOP, g_hWnd, (LPPOINT)&rect, 2);
				
					rect.left += delta.x;
					rect.right += delta.x;

					::SetWindowPos(		hwnd, HWND_TOP, 
										rect.left, rect.top, 
										rect.right - rect.left, rect.bottom - rect.top, 
										SWP_NOSIZE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS);
				}

				for (auto ID : bottomAligned)
				{
					HWND hwnd = GetDlgItem(g_hWnd, ID);

					GetWindowRect(hwnd, &rect);
					MapWindowPoints(HWND_DESKTOP, g_hWnd, (LPPOINT)&rect, 2);
				
					rect.top += delta.y;
					rect.bottom += delta.y;

					::SetWindowPos(		hwnd, HWND_TOP, 
										rect.left, rect.top, 
										rect.right - rect.left, rect.bottom - rect.top, 
										SWP_NOSIZE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS);
				}

				g_PreviousSize.x = width;
				g_PreviousSize.y = height;

				UpdateThumbnail();
			}

			break;
		}
    }

    return FALSE;
}

//---------------------------------------------------------------------------------------------------
// PopulateListbox
//---------------------------------------------------------------------------------------------------
void PopulateListbox(const DocumentList& documents)
{
	ListView_DeleteAllItems(g_ListBox);

	LV_ITEM listItem;
	memset(&listItem, 0, sizeof(listItem));
	listItem.mask = LVIF_TEXT;
	listItem.cchTextMax = 256;
	
	SendMessage(g_ListBox, WM_SETREDRAW, FALSE, 0);

	int selectionIndex = 0;

	int count = 0;
	for (DocumentList::const_iterator itr = documents.begin(); itr != documents.end(); ++itr)
	{
		listItem.mask |= LVIF_PARAM;
		listItem.iItem = count;
		listItem.iSubItem = 0;
		listItem.lParam = (LPARAM)(*itr);
		listItem.pszText = (LPWSTR)(*itr)->m_path.c_str();
		ListView_InsertItem(g_ListBox, &listItem);

		listItem.mask &= ~LVIF_PARAM;
		listItem.iItem = count;
		listItem.iSubItem = 1;
		listItem.pszText = (LPWSTR)(*itr)->m_authors.c_str();
		ListView_SetItem(g_ListBox, &listItem);

		listItem.mask &= ~LVIF_PARAM;
		listItem.iItem = count;
		listItem.iSubItem = 2;
		listItem.pszText = (LPWSTR)(*itr)->m_company.c_str();
		ListView_SetItem(g_ListBox, &listItem);

		if ((*itr) == g_CurrentDocument)
		{
			selectionIndex = count;
		}

		++count;
	}

	ListView_SetColumnWidth(g_ListBox, 0, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(g_ListBox, 1, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(g_ListBox, 2, LVSCW_AUTOSIZE);

	SendMessage(g_ListBox, WM_SETREDRAW, TRUE, 0);
	RedrawWindow(g_ListBox, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

	ListView_EnsureVisible(g_ListBox, selectionIndex, FALSE);
	ListView_SetItemState(g_ListBox, selectionIndex, LVIS_SELECTED, LVIS_SELECTED);

	SortList();


	EnableWindow(g_KeywordEditBox, (count > 0));

	if (count == 0)
	{
		g_CurrentDocument = NULL;
		SetWindowText(g_KeywordEditBox, L"here be dragons");
	}

	TCHAR buffer[64];
	swprintf(buffer, 64, L"Results : %d", count);
	SetWindowText(g_StatusBar, buffer);
}

//---------------------------------------------------------------------------------------------------
// ProcessFilterEditBox
//---------------------------------------------------------------------------------------------------
void ProcessFilterEditBox()
{
	TCHAR buffer[1024];
	GetWindowText(g_FilterEditBox, buffer, 1024);

	std::wstring filter = buffer;

	if (filter.length() <= 1 || g_CurrentSelection.Empty() || filter.find(g_CurrentFilter) != 0)
	{
		g_CurrentSelection = g_Library->Filter(filter.c_str());
	}
	else
	{
		g_CurrentSelection = g_CurrentSelection.Filter(filter.c_str());
	}

	g_CurrentFilter = filter;

	PopulateListbox(g_CurrentSelection.GetDocuments());
}

//---------------------------------------------------------------------------------------------------
// CompareFunc
//---------------------------------------------------------------------------------------------------
int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	Document* a = g_SortAscending ? (Document*)lParam1 : (Document*)lParam2;
	Document* b = g_SortAscending ? (Document*)lParam2 : (Document*)lParam1;
	
	if (g_SortIndex == 0)
		return wcscmp(a->m_pathLower.c_str(), b->m_pathLower.c_str());
	else if (g_SortIndex == 1)
		return wcscmp(a->m_authors.c_str(), b->m_authors.c_str());
	else if (g_SortIndex == 2)
		return wcscmp(a->m_company.c_str(), b->m_company.c_str());
	return 0;
}

//---------------------------------------------------------------------------------------------------
// SortList
//---------------------------------------------------------------------------------------------------
void SortList()
{
	int			g_SortIndex = 0;
	bool		g_SortAscending = true;
	ListView_SortItems(g_ListBox, CompareFunc, NULL);
}

//---------------------------------------------------------------------------------------------------
// Rescan
//---------------------------------------------------------------------------------------------------
void Rescan()
{
	g_Library->SaveMeta();
	g_Library->Clear();
	g_Library->LoadMeta();
	g_Library->Scan();
	g_Library->SaveMeta();

	g_CurrentSelection = g_Library->Filter(L"");
	ProcessFilterEditBox();
}

//---------------------------------------------------------------------------------------------------
// AddDlgProc
//---------------------------------------------------------------------------------------------------
BOOL CALLBACK AddDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDOK:
				{
					TCHAR buffer[256];

					if (GetDlgItemText(hwndDlg, IDC_EDIT_ADD, buffer, 256))
					{
						g_AddDialogPath = buffer;
					}

					EndDialog(hwndDlg, wParam);
					return TRUE;

					break;
				}

				case IDCANCEL:
				{
					EndDialog(hwndDlg, wParam);
					return TRUE;
				}
				break;
			}
			break;
		}
	}
	return FALSE;
}

//---------------------------------------------------------------------------------------------------
// CopyToClipboard
//---------------------------------------------------------------------------------------------------
void CopyToClipboard()
{
	if (!g_CurrentDocument)
		return;

	if (!OpenClipboard(g_hWnd))
		return;

	EmptyClipboard();

	HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (g_CurrentDocument->m_path.size()+1) * sizeof(TCHAR));

	if (hglbCopy == NULL)
	{
		CloseClipboard();
		return;
	}

	unsigned char* ptr = (unsigned char*)GlobalLock(hglbCopy);
	{
		memcpy(ptr, g_CurrentDocument->m_path.c_str(), g_CurrentDocument->m_path.size() * sizeof(TCHAR));
		ptr[g_CurrentDocument->m_path.size() * sizeof(TCHAR)] = 0;
	}
	GlobalUnlock(hglbCopy);

	SetClipboardData(CF_UNICODETEXT, hglbCopy);

	CloseClipboard();
}

//---------------------------------------------------------------------------------------------------
// UpdateThumbnail
// code lifted from https://github.com/pauldotknopf/WindowsSDK7-Samples/tree/master/winui/shell/appplatform/UsingImageFactory
//---------------------------------------------------------------------------------------------------
void UpdateThumbnail()
{
	if (!g_CurrentDocument)
		return;

	HRESULT hr;

	// Getting the IShellItemImageFactory interface pointer for the file.
	IShellItemImageFactory *pImageFactory;
	hr = SHCreateItemFromParsingName(g_CurrentDocument->m_path.c_str(), NULL, IID_PPV_ARGS(&pImageFactory));

	if (SUCCEEDED(hr))
	{
		SIZE size = { 256, 256 };

		//sz - Size of the image, SIIGBF_BIGGERSIZEOK - GetImage will stretch down the bitmap (preserving aspect ratio)
		HBITMAP hbmp;
		hr = pImageFactory->GetImage(size, SIIGBF_RESIZETOFIT, &hbmp);
		//hr = pImageFactory->GetImage(size, SIIGBF_BIGGERSIZEOK, &hbmp);

		if (SUCCEEDED(hr))
		{
			SendDlgItemMessage(g_hWnd, IDC_THUMBNAIL, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmp);
			DeleteObject(hbmp);
		}
	
		pImageFactory->Release();
	}
}
