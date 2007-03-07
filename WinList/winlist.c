//
//  VirtuaWin - Virtual Desktop Manager (virtuawin.sourceforge.net)
//  winlist.c - VirtuaWin module for restoring lost windows.
// 
//  Copyright (c) 1999-2005 Johan Piculell
//  Copyright (c) 2006 VirtuaWin (VirtuaWin@home.se)
// 
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
//  USA.
//

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <tchar.h>

#include "winlistres.h"
#include "../Defines.h"
#include "../Messages.h"

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES	((DWORD)-1)
#endif

HINSTANCE hInst;   // Instance handle
HWND hwndMain;	   // Main window handle
HWND vwHandle;     // Handle to VirtuaWin

typedef struct {
    HWND handle;
    RECT rect;
    int  desk;
    int  style;
    int  exstyle;
    int  restored;
} winType;

winType windowList[999]; // should be enough
TCHAR userAppPath[MAX_PATH] ;
int initialised=0 ;
int noOfWin;
int screenLeft;
int screenRight;
int screenTop;
int screenBottom;
UINT RM_Shellhook;
HWND hwndTask;          // handle to taskbar

/* prototype for the dialog box function. */
static BOOL CALLBACK DialogFunc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* Main message handler */
LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

static void goGetTheTaskbarHandle(void)
{
    HWND hwndTray = FindWindowEx(NULL, NULL,_T("Shell_TrayWnd"), NULL);
    HWND hwndBar = FindWindowEx(hwndTray, NULL,_T("ReBarWindow32"), NULL );
    
    // Maybe "RebarWindow32" is not a child to "Shell_TrayWnd", then try this
    if(hwndBar == NULL)
        hwndBar = hwndTray;
    
    hwndTask = FindWindowEx(hwndBar, NULL,_T("MSTaskSwWClass"), NULL);
    
    if(hwndTask == NULL)
        MessageBox(hwndMain,_T("Could not locate handle to the taskbar.\n This will disable the ability to hide troublesome windows correctly."), _T("VirtuaWinList Error"), MB_ICONWARNING); 
}

/* Initializes the window */ 
static BOOL WinListInit(void)
{
    WNDCLASS wc;
    
    memset(&wc, 0, sizeof(WNDCLASS));
    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC)MainWndProc;
    wc.hInstance = hInst;
    /* IMPORTANT! The classname must be the same as the filename since VirtuaWin uses 
       this for locating the window */
    wc.lpszClassName = _T("WinList.exe");
    
    if (!RegisterClass(&wc))
        return 0;
    
    // the window is never shown
    if ((hwndMain = CreateWindow(_T("WinList.exe"), 
                                 _T("WinList"), 
                                 WS_POPUP,
                                 CW_USEDEFAULT, 
                                 0, 
                                 CW_USEDEFAULT, 
                                 0,
                                 NULL,
                                 NULL,
                                 hInst,
                                 NULL)) == (HWND)0)
        return 0;
    
    RM_Shellhook = RegisterWindowMessage(_T("SHELLHOOK"));
    goGetTheTaskbarHandle() ;
    
    return 1;
}

static VOID CALLBACK startupFailureTimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
    if(!initialised)
    {
        MessageBox(hwnd, _T("VirtuaWin failed to send the UserApp path."), _T("VirtuaWinList Error"), MB_ICONWARNING);
        exit(1) ;
    }
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        
    case MOD_INIT: // This must be taken care of in order to get the handle to VirtuaWin. 
        // The handle to VirtuaWin comes in the wParam 
        vwHandle = (HWND) wParam; // Should be some error handling here if NULL 
        if(!initialised)
        {
            // Get the VW user's path - give VirtuaWin 10 seconds to do this
            SendMessage(vwHandle, VW_USERAPPPATH, 0, 0);
            SetTimer(hwnd, 0x29a, 10000, startupFailureTimerProc);
        }
        break;
    case WM_COPYDATA:
        if(!initialised)
        {
            COPYDATASTRUCT *cds;         
            cds = (COPYDATASTRUCT *) lParam ;         
            if(cds->dwData == (0-VW_USERAPPPATH))
            {
                if((cds->cbData < 2) || (cds->lpData == NULL))
                {
                    MessageBox(hwnd, _T("VirtuaWin returned a bad UserApp path."), _T("VirtuaWinList Error"), MB_ICONWARNING);
                    exit(1) ;
                }
#ifdef _UNICODE
                MultiByteToWideChar(CP_ACP,0,(char *) cds->lpData,-1,userAppPath,MAX_PATH) ;
#else
                strcpy(userAppPath,(char *) cds->lpData) ;
#endif
                initialised = 1 ;
            }
        }
        return TRUE ;
    case MOD_QUIT: // This must be handeled, otherwise VirtuaWin can't shut down the module 
        PostQuitMessage(0);
        break;
    case MOD_SETUP: // Optional
        DialogBox(hInst, MAKEINTRESOURCE(IDD_MAINDIALOG), vwHandle, (DLGPROC) DialogFunc);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

/*
 * Main startup function
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
    MSG msg;
    hInst = hInstance;
    if(!WinListInit())
        return 0;
    
    // main messge loop
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

/*************************************************
 * Callback function. Integrates all enumerated windows
 */
__inline BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) 
{
    TCHAR *ss, buff[vwCLASSNAME_MAX+vwWINDOWNAME_MAX+4];
    int desk, exstyle, style = GetWindowLong(hwnd, GWL_STYLE);
    HWND phwnd, gphwnd ;
    RECT rect ;
    
    if(style & WS_CHILD)
        return TRUE ;

    GetWindowRect(hwnd,&rect);
    exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if((desk = SendMessage(vwHandle,VW_GETWINDESK,(WPARAM) hwnd,0)) > 0)
        ;
    else if((((phwnd=GetParent(hwnd)) == NULL) || (phwnd == GetDesktopWindow()) ||
             (!(GetWindowLong(phwnd, GWL_STYLE) & WS_VISIBLE) &&
              (((gphwnd=GetParent(phwnd)) == NULL) || (gphwnd == GetDesktopWindow())))) &&
            (!(exstyle & WS_EX_TOOLWINDOW) || (!(style & WS_POPUP) && (GetWindow(hwnd,GW_OWNER) != NULL)) || (rect.top < -5000)))
        ;
    else
        return TRUE ;
        
    ss = buff ;
    // Add window to the windowlist
    if(desk > 0)
        // This is a currently managed window
        *ss++ = '0' + desk ;
    else if((rect.top < -5000) && (rect.top > -30000))
        // If at the hidden position then flag as likely lost
        *ss++ = '*' ;
    else if(style & WS_VISIBLE)
        *ss++ = ' ' ;
    else
        *ss++ = 'H' ;
    *ss++ = '\t' ;
    GetClassName(hwnd,ss,vwCLASSNAME_MAX);
    ss += _tcslen(ss) ;
    *ss++ = '\t' ;
    if(!GetWindowText(hwnd,ss,vwWINDOWNAME_MAX))
        _tcscpy(ss,_T("<None>"));
    SendDlgItemMessage((HWND) lParam, ID_WINLIST, LB_ADDSTRING, 0, (LONG) buff);
    windowList[noOfWin].handle = hwnd;
    windowList[noOfWin].style = style;
    windowList[noOfWin].exstyle = exstyle;
    windowList[noOfWin].desk = desk ;
    windowList[noOfWin].rect = rect ;
    windowList[noOfWin].restored = 0 ;
    noOfWin++;
    
    return TRUE;
}

BOOL CALLBACK enumWindowsSaveListProc(HWND hwnd, LPARAM lParam) 
{
    FILE *wdFp=(FILE *) lParam ;
    DWORD procId, threadId ;
    int style, exstyle ;
    RECT pos ;
#ifdef _UNICODE
    WCHAR nameW[vwWINDOWNAME_MAX] ;
#endif
    char text[vwWINDOWNAME_MAX] ;
    char class[vwCLASSNAME_MAX] ;
    
    
    style = GetWindowLong(hwnd, GWL_STYLE);
    exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    threadId = GetWindowThreadProcessId(hwnd,&procId) ;
    GetWindowRect(hwnd,&pos);
#ifdef _UNICODE
    GetClassName(hwnd,nameW,vwCLASSNAME_MAX);
    WideCharToMultiByte(CP_ACP,0,nameW,-1,class,vwCLASSNAME_MAX, 0, 0) ;
    if(GetWindowText(hwnd,nameW,vwWINDOWNAME_MAX))
        WideCharToMultiByte(CP_ACP,0,nameW,-1,text,vwWINDOWNAME_MAX, 0, 0) ;
    else
#else
    GetClassName(hwnd,class,vwCLASSNAME_MAX);
    if(!GetWindowText(hwnd,text,vwWINDOWNAME_MAX))
#endif
        strcpy(text,"<None>");
    
    fprintf(wdFp,"%8x %08x %08x %8x %8x %8x %s\n%8d %8x %8d %8d %8d %8d %s\n",
            (int)hwnd,style,exstyle,(int)GetParent(hwnd),
            (int)GetWindow(hwnd,GW_OWNER),(int)GetClassLong(hwnd,GCL_HICON),text,
            (int)procId,(int)threadId,(int)pos.top,(int)pos.bottom,(int)pos.left,(int)pos.right,class) ;
    
    return TRUE;
}

/*
 */
static int GenerateWinList(HWND hDlg)
{
    /* The virtual screen size system matrix values were only added for WINVER >= 0x0500 (Win2k) */
#ifndef SM_XVIRTUALSCREEN
#define SM_XVIRTUALSCREEN       76
#define SM_YVIRTUALSCREEN       77
#define SM_CXVIRTUALSCREEN      78
#define SM_CYVIRTUALSCREEN      79
#endif
    int tabstops[2] ;
    if((screenRight  = GetSystemMetrics(SM_CXVIRTUALSCREEN)) <= 0)
    {
        /* The virtual screen size system matrix values are not supported on
         * this OS (Win95 & NT), use the desktop window size */
        RECT r;
        GetClientRect(GetDesktopWindow(), &r);
        screenLeft   = r.left;
        screenRight  = r.right;
        screenTop    = r.top;
        screenBottom = r.bottom;
    }
    else
    {
        screenLeft   = GetSystemMetrics(SM_XVIRTUALSCREEN);
        screenRight += screenLeft;
        screenTop    = GetSystemMetrics(SM_YVIRTUALSCREEN);
        screenBottom = GetSystemMetrics(SM_CYVIRTUALSCREEN)+screenTop;
    }
    SendDlgItemMessage(hDlg,ID_WINLIST,LB_SETHORIZONTALEXTENT,vwCLASSNAME_MAX+vwCLASSNAME_MAX+4, 0);
    tabstops[0] = 14 ;
    tabstops[1] = 140 ;
    SendDlgItemMessage(hDlg,ID_WINLIST,LB_SETTABSTOPS,(WPARAM)2,(LPARAM)tabstops);
    noOfWin = 0;
    EnumWindows(enumWindowsProc, (LPARAM) hDlg);   // get all windows
    return 1;
}

/*
   This is the main function for the dialog. 
 */
static BOOL CALLBACK DialogFunc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int curSel;
    switch (msg) {
    case WM_INITDIALOG:
        GenerateWinList(hwndDlg);
        return TRUE;
        
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDREFRESH:
            SendDlgItemMessage(hwndDlg,ID_WINLIST,LB_RESETCONTENT,0, 0);
            GenerateWinList(hwndDlg) ;
            return TRUE;
            
        case IDOK:
            curSel = SendDlgItemMessage(hwndDlg, ID_WINLIST, LB_GETCURSEL, 0, 0);
            if(curSel != LB_ERR)
            {
                int left, top;
                if(windowList[curSel].desk > 0)
                    /* make VW display the window */
                    SendMessage(vwHandle,VW_ACCESSWIN,(WPARAM) windowList[curSel].handle,1) ;
                if((windowList[curSel].style & WS_VISIBLE) == 0)
                {
                    ShowWindow(windowList[curSel].handle, SW_SHOWNA);
                    ShowOwnedPopups(windowList[curSel].handle, SW_SHOWNA);
                }
                if((windowList[curSel].style & WS_VISIBLE) && (windowList[curSel].exstyle & WS_EX_TOOLWINDOW))
                {
                    // Restore the window mode
                    SetWindowLong(windowList[curSel].handle, GWL_EXSTYLE, (windowList[curSel].exstyle & (~WS_EX_TOOLWINDOW))) ;  
                    // Notify taskbar of the change
                    PostMessage(hwndTask, RM_Shellhook, 1, (LPARAM) windowList[curSel].handle);
                }
                left = windowList[curSel].rect.left ;
                top = windowList[curSel].rect.top ;
                /* some apps hide the window by pushing it to -32000,
                 * VirtuaWin does not move these windows */
                if((left != -32000) || (top != -32000))
                {
                    if(top < -5000)
                        top += 25000 ;
                    if(left < screenLeft)
                        left = screenLeft + 10 ;
                    else if(left > screenRight)
                        left = screenLeft + 10 ;
                    if(top < screenTop)
                        top = screenTop + 10 ;
                    else if(top > screenBottom)
                        top = screenTop + 10 ;
                    SetWindowPos(windowList[curSel].handle, 0, left, top, 0, 0,
                                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE ); 
                }
                SetForegroundWindow(windowList[curSel].handle);
                windowList[curSel].restored = 1 ;
            }
            return 1;
        case IDUNDO:
            curSel = SendDlgItemMessage(hwndDlg, ID_WINLIST, LB_GETCURSEL, 0, 0);
            if(curSel != LB_ERR)
            {
                if(windowList[curSel].restored)
                {
                    if(windowList[curSel].desk > 0)
                        /* make VW display the window */
                        SendMessage(vwHandle,VW_ASSIGNWIN,(WPARAM) windowList[curSel].handle,windowList[curSel].desk) ;
                    if((windowList[curSel].style & WS_VISIBLE) == 0)
                    {
                        ShowWindow(windowList[curSel].handle, SW_HIDE);
                        ShowOwnedPopups(windowList[curSel].handle, SW_HIDE);
                    }
                    if((windowList[curSel].style & WS_VISIBLE) && (windowList[curSel].exstyle & WS_EX_TOOLWINDOW))
                    {
                        // Restore the window mode
                        SetWindowLong(windowList[curSel].handle, GWL_EXSTYLE, windowList[curSel].exstyle) ;  
                        // Notify taskbar of the change
                        PostMessage( hwndTask, RM_Shellhook, 2, (LPARAM) windowList[curSel].handle);
                    }
                    SetWindowPos(windowList[curSel].handle, 0, windowList[curSel].rect.left, windowList[curSel].rect.top, 0, 0,
                                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE ); 
                    windowList[curSel].restored = 0 ;
                }
                else
                    MessageBox(hwndDlg, _T("Cannot undo, not restored."), _T("VirtuaWinList Error"), MB_ICONWARNING);
            }
            return 1;
        
        case IDSAVE:
            {
                TCHAR fname[MAX_PATH] ;
                FILE *wdFp ;
                int ii=1 ;
                while(ii < 1000)
                {
                    _stprintf(fname,_T("%sWinList_%d.log"),userAppPath,ii) ;
                    if(GetFileAttributes(fname) == INVALID_FILE_ATTRIBUTES)
                        break ;
                    ii++ ;
                }
                if(ii == 1000)
                    MessageBox(hwndDlg, _T("Cannot create a WinList_#.log file, please clean up your user directory."), _T("VirtuaWinList Error"), MB_ICONWARNING);
                else
                {
                    wdFp = _tfopen(fname,_T("w+")) ;
                    EnumWindows(enumWindowsSaveListProc,(LPARAM) wdFp) ;
                    fclose(wdFp) ;
                }
            }
            return 1;
        case IDCANCEL:
            EndDialog(hwndDlg,0);
            return 1;
        }
        break;
        
    case WM_CLOSE:
        EndDialog(hwndDlg,0);
        return TRUE;
	
    }
    return FALSE;
}
