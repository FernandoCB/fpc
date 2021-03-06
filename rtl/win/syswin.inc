{
    This file is part of the Free Pascal run time library.
    Copyright (c) 1999-2008 by Florian Klaempfl and Pavel Ozerski
    member of the Free Pascal development team.

    FPC Pascal system unit part shared by win32/win64.

    See the file COPYING.FPC, included in this distribution,
    for details about the copyright.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 **********************************************************************}


Const
   DLL_PROCESS_ATTACH = 1;
   DLL_THREAD_ATTACH = 2;
   DLL_PROCESS_DETACH = 0;
   DLL_THREAD_DETACH = 3;
   DLLExitOK : boolean = true;
Var
  DLLBuf : Jmp_buf;

function Dll_entry{$ifdef FPC_HAS_INDIRECT_MAIN_INFORMATION}(const info : TEntryInformation){$endif FPC_HAS_INDIRECT_MAIN_INFORMATION} : longbool; [public,alias:'_FPC_DLL_Entry'];
  begin
{$ifdef FPC_HAS_INDIRECT_MAIN_INFORMATION}
     EntryInformation:=info;
{$endif FPC_HAS_INDIRECT_MAIN_INFORMATION}
     IsLibrary:=true;
     Dll_entry:=false;  { return value is ignored, except when DLLreason=DLL_PROCESS_ATTACH }
     case DLLreason of
       DLL_PROCESS_ATTACH :
         begin
           MainThreadIdWin32 := Win32GetCurrentThreadId;

           If SetJmp(DLLBuf) = 0 then
             begin
{$ifdef FPC_HAS_INDIRECT_MAIN_INFORMATION}
               EntryInformation.PascalMain();
{$else FPC_HAS_INDIRECT_MAIN_INFORMATION}
               PascalMain;
{$endif FPC_HAS_INDIRECT_MAIN_INFORMATION}
               Dll_entry:=true;
             end
           else
             Dll_entry:=DLLExitOK;
         end;
       DLL_THREAD_ATTACH :
         begin
           { SysInitMultithreading must not be called here,
             see comments in exec_tls_callback below }
           { Allocate Threadvars  }
           SysAllocateThreadVars;

           { NS : no idea what is correct to pass here - pass dummy value for now }
           { passing a dummy is ok, the correct value is read from the coff header of SysInstance (FK) }
           InitThread($1000000); { Assume everything is idempotent there, as the thread could have been created with BeginThread... }

           if assigned(Dll_Thread_Attach_Hook) then
             Dll_Thread_Attach_Hook(DllParam);
        end;
       DLL_THREAD_DETACH :
         begin
           if assigned(Dll_Thread_Detach_Hook) then
             Dll_Thread_Detach_Hook(DllParam);
           { Release Threadvars }
           if TlsGetValue(TLSKey)<>nil then
             DoneThread; { Assume everything is idempotent there }
         end;
       DLL_PROCESS_DETACH :
         begin
	   if MainThreadIDWin32=0 then // already been here.
	     exit;
           If SetJmp(DLLBuf) = 0 then
             FPC_Do_Exit;
           if assigned(Dll_Process_Detach_Hook) then
             Dll_Process_Detach_Hook(DllParam);

           SysReleaseThreadVars;
           { Free TLS resources used by ThreadVars }
           SysFiniMultiThreading;
           MainThreadIDWin32:=0;
         end;
     end;
  end;


Procedure ExitDLL(Exitcode : longint);
  begin
    DLLExitOK:=ExitCode=0;
    LongJmp(DLLBuf,1);
  end;

{$ifdef FPC_USE_TLS_DIRECTORY}
{ Process TLS callback function }
{ This is only useful for executables
  for DLLs, DLL_Entry gets called. PM }

procedure Exec_Tls_callback(Handle : pointer; reason : Dword; Reserved : pointer);
  stdcall; [public,alias:'_FPC_Tls_Callback'];
  begin
     if IsLibrary then
       Exit;
     case reason of
       { For executables, DLL_PROCESS_ATTACH is called *before* the entry point,
         and DLL_PROCESS_DETACH is called *after* RTL shuts down and calls ExitProcess.
         It isn't a good idea to handle resources of the main thread at these points. }
       DLL_THREAD_ATTACH :
         begin
         {  !!! SysInitMultithreading must NOT be called here. Windows guarantees that
            the main thread invokes PROCESS_ATTACH, not THREAD_ATTACH. So this always
            executes in non-main thread. SysInitMultithreading() here will cause
            initial threadvars to be copied to TLS of non-main thread, and threadvars
            of the main thread will be reinitialized upon the next access with zeroes,
            ending up in a delayed failure which is very hard to debug.
            Fortunately this nasty scenario can happen only when the first non-main thread
            was created outside of RTL (Sergei).
         }
           { Allocate Threadvars  }
           SysAllocateThreadVars;

           { NS : no idea what is correct to pass here - pass dummy value for now }
           { passing a dummy is ok, the correct value is read from the coff header of SysInstance (FK) }
           InitThread($1000000); { Assume everything is idempotent there, as the thread could have been created with BeginThread... }
         end;
       DLL_THREAD_DETACH :
         begin
           if TlsGetValue(TLSKey)<>nil then
             DoneThread; { Assume everything is idempotent there }
         end;
     end;
  end;

const
  FreePascal_TLS_callback : pointer = @Exec_Tls_callback;
    public name '__FPC_tls_callbacks' section '.CRT$XLFPC';
  FreePascal_end_of_TLS_callback : pointer = nil;
    public name '__FPC_end_of_tls_callbacks' section '.CRT$XLZZZ';
var
  tls_callbacks : pointer; external name '___crt_xl_start__';
  tls_data_start : pointer; external name '___tls_start__';
  tls_data_end : pointer; external name '___tls_end__';

  _tls_index : dword; cvar; external;

const
  _tls_used : TTlsDirectory = (
    data_start : @tls_data_start;
    data_end : @tls_data_end;
    index_pointer : @_tls_index;
    callbacks_pointer : @tls_callbacks;
    zero_fill_size : 0;
    flags : 0;
  ); cvar; public;

{$ifdef win64}
  { This is a hack to support external linking.
    All released win64 versions of GNU binutils miss proper prefix handling
    when searching for _tls_used and expect two leading underscores.
    The issue has been fixed in binutils snapshots, but not released yet.

    TODO: This should be removed as soon as next version of binutils (>2.21) is
    released and we upgrade to it. }
    __tls_used : TTlsDirectory = (
      data_start : @tls_data_start;
      data_end : @tls_data_end;
      index_pointer : @_tls_index;
      callbacks_pointer : @tls_callbacks;
      zero_fill_size : 0;
      flags : 0;
    ); cvar; public;
{$endif win64}
{$endif FPC_USE_TLS_DIRECTORY}


{****************************************************************************
                    Error Message writing using messageboxes
****************************************************************************}

function MessageBox(w1:longint;l1,l2:pointer;w2:longint):longint;
   stdcall;external 'user32' name 'MessageBoxA';

const
  ErrorBufferLength = 1024;
var
  ErrorBuf : array[0..ErrorBufferLength] of char;
  ErrorLen : SizeInt;

Function ErrorWrite(Var F: TextRec): Integer;
{
  An error message should always end with #13#10#13#10
}
var
  i : SizeInt;
Begin
  while F.BufPos>0 do
    begin
      begin
        if F.BufPos+ErrorLen>ErrorBufferLength then
          i:=ErrorBufferLength-ErrorLen
        else
          i:=F.BufPos;
        Move(F.BufPtr^,ErrorBuf[ErrorLen],i);
        inc(ErrorLen,i);
        ErrorBuf[ErrorLen]:=#0;
      end;
      if ErrorLen=ErrorBufferLength then
        begin
          MessageBox(0,@ErrorBuf,pchar('Error'),0);
          ErrorLen:=0;
        end;
      Dec(F.BufPos,i);
    end;
  ErrorWrite:=0;
End;


Function ErrorClose(Var F: TextRec): Integer;
begin
  if ErrorLen>0 then
   begin
     MessageBox(0,@ErrorBuf,pchar('Error'),0);
     ErrorLen:=0;
   end;
  ErrorLen:=0;
  ErrorClose:=0;
end;


Function ErrorOpen(Var F: TextRec): Integer;
Begin
  TextRec(F).InOutFunc:=@ErrorWrite;
  TextRec(F).FlushFunc:=@ErrorWrite;
  TextRec(F).CloseFunc:=@ErrorClose;
  ErrorLen:=0;
  ErrorOpen:=0;
End;


procedure AssignError(Var T: Text);
begin
  Assign(T,'');
  TextRec(T).OpenFunc:=@ErrorOpen;
  Rewrite(T);
end;


procedure SysInitStdIO;
begin
  { Setup stdin, stdout and stderr, for GUI apps redirect stderr,stdout to be
    displayed in a messagebox }
  StdInputHandle:=longint(GetStdHandle(cardinal(STD_INPUT_HANDLE)));
  StdOutputHandle:=longint(GetStdHandle(cardinal(STD_OUTPUT_HANDLE)));
  StdErrorHandle:=longint(GetStdHandle(cardinal(STD_ERROR_HANDLE)));
  if not IsConsole then
   begin
     AssignError(stderr);
     AssignError(StdOut);
     Assign(Output,'');
     Assign(Input,'');
     Assign(ErrOutput,'');
   end
  else
   begin
     OpenStdIO(Input,fmInput,StdInputHandle);
     OpenStdIO(Output,fmOutput,StdOutputHandle);
     OpenStdIO(ErrOutput,fmOutput,StdErrorHandle);
     OpenStdIO(StdOut,fmOutput,StdOutputHandle);
     OpenStdIO(StdErr,fmOutput,StdErrorHandle);
   end;
end;

{ ProcessID cached to avoid repeated calls to GetCurrentProcess. }

var
  ProcessID: SizeUInt;

function GetProcessID: SizeUInt;
  begin
    GetProcessID := ProcessID;
  end;


{******************************************************************************
                              Unicode
 ******************************************************************************}
 const
   { MultiByteToWideChar  }
      MB_PRECOMPOSED = 1;
      CP_ACP = 0;
      WC_NO_BEST_FIT_CHARS = $400;

 function MultiByteToWideChar(CodePage:UINT; dwFlags:DWORD; lpMultiByteStr:PChar; cchMultiByte:longint; lpWideCharStr:PWideChar;cchWideChar:longint):longint;
     stdcall; external 'kernel32' name 'MultiByteToWideChar';
 function WideCharToMultiByte(CodePage:UINT; dwFlags:DWORD; lpWideCharStr:PWideChar; cchWideChar:longint; lpMultiByteStr:PChar;cchMultiByte:longint; lpDefaultChar:PChar; lpUsedDefaultChar:pointer):longint;
     stdcall; external 'kernel32' name 'WideCharToMultiByte';
 function CharUpperBuff(lpsz:LPWSTR; cchLength:DWORD):DWORD;
     stdcall; external 'user32' name 'CharUpperBuffW';
 function CharLowerBuff(lpsz:LPWSTR; cchLength:DWORD):DWORD;
     stdcall; external 'user32' name 'CharLowerBuffW';

procedure Win32Unicode2AnsiMove(source:punicodechar;var dest:ansistring;len:SizeInt);
  var
    destlen: SizeInt;
  begin
    // retrieve length including trailing #0
    // not anymore, because this must also be usable for single characters
    destlen:=WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, source, len, nil, 0, nil, nil);
    // this will null-terminate
    setlength(dest, destlen);
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, source, len, @dest[1], destlen, nil, nil);
  end;

procedure Win32Ansi2UnicodeMove(source:pchar;var dest:UnicodeString;len:SizeInt);
  var
    destlen: SizeInt;
  begin
    // retrieve length including trailing #0
    // not anymore, because this must also be usable for single characters
    destlen:=MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, source, len, nil, 0);
    // this will null-terminate
    setlength(dest, destlen);
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, source, len, @dest[1], destlen);
  end;


function Win32UnicodeUpper(const s : UnicodeString) : UnicodeString;
  begin
    result:=s;
    UniqueString(result);
    if length(result)>0 then
      CharUpperBuff(LPWSTR(result),length(result));
  end;


function Win32UnicodeLower(const s : UnicodeString) : UnicodeString;
  begin
    result:=s;
    UniqueString(result);
    if length(result)>0 then
      CharLowerBuff(LPWSTR(result),length(result));
  end;

{******************************************************************************
                              Widestring
 ******************************************************************************}

procedure Win32Ansi2WideMove(source:pchar;var dest:widestring;len:SizeInt);
  var
    destlen: SizeInt;
  begin
    // retrieve length including trailing #0
    // not anymore, because this must also be usable for single characters
    destlen:=MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, source, len, nil, 0);
    // this will null-terminate
    setlength(dest, destlen);
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, source, len, @dest[1], destlen);
  end;


function Win32WideUpper(const s : WideString) : WideString;
  begin
    result:=s;
    if length(result)>0 then
      CharUpperBuff(LPWSTR(result),length(result));
  end;


function Win32WideLower(const s : WideString) : WideString;
  begin
    result:=s;
    if length(result)>0 then
      CharLowerBuff(LPWSTR(result),length(result));
  end;

type
  PWStrInitEntry = ^TWStrInitEntry;
  TWStrInitEntry = record
    addr: PPointer;
    data: Pointer;
  end;

  PWStrInitTablesTable = ^TWStrInitTablesTable;
  TWStrInitTablesTable = packed record
    count  : longint;
    tables : packed array [1..32767] of PWStrInitEntry;
  end;

{$if not(defined(VER2_2) or defined(VER2_4))}
var
  WStrInitTablesTable: TWStrInitTablesTable; external name 'FPC_WIDEINITTABLES';
{$endif}

{ there is a similiar procedure in sysutils which inits the fields which
  are only relevant for the sysutils units }
procedure InitWin32Widestrings;
  var
    i: longint;
    ptable: PWStrInitEntry;
  begin
{$if not(defined(VER2_2) or defined(VER2_4))}
    { assign initial values to global Widestring typed consts }
    for i:=1 to WStrInitTablesTable.count do
      begin
        ptable:=WStrInitTablesTable.tables[i];
        while Assigned(ptable^.addr) do
          begin
            fpc_widestr_assign(ptable^.addr^, ptable^.data);
            Inc(ptable);
          end;
      end;
{$endif}

    { Note: since WideChar=UnicodeChar and PWideChar=PUnicodeChar,
      Wide2AnsiMoveProc is identical to Unicode2AnsiStrMoveProc. }

    { Widestring }
    widestringmanager.Wide2AnsiMoveProc:=@Win32Unicode2AnsiMove;
    widestringmanager.Ansi2WideMoveProc:=@Win32Ansi2WideMove;
    widestringmanager.UpperWideStringProc:=@Win32WideUpper;
    widestringmanager.LowerWideStringProc:=@Win32WideLower;
{$ifndef VER2_2}
    { Unicode }
    widestringmanager.Unicode2AnsiMoveProc:=@Win32Unicode2AnsiMove;
    widestringmanager.Ansi2UnicodeMoveProc:=@Win32Ansi2UnicodeMove;
    widestringmanager.UpperUnicodeStringProc:=@Win32UnicodeUpper;
    widestringmanager.LowerUnicodeStringProc:=@Win32UnicodeLower;
{$endif VER2_2}
  end;

