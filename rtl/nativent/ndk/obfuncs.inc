{%MainUnit ndk.pas}
{
    Native Development Kit for Native NT

    This file is part of the Free Pascal run time library.
    This unit contains functions for use with the Object Manager.
    Copyright (c) 2010 by Sven Barth

    See the file COPYING.FPC, included in this distribution,
    for details about the copyright.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 **********************************************************************}

function NtOpenDirectoryObject(
  FileHandle: PHANDLE;
  DesiredAccess: ACCESS_MASK;
  ObjectAttributes: POBJECT_ATTRIBUTES
): NTSTATUS; external ntdll;

function NtWaitForSingleObject(
  _Object: HANDLE;
  Alertable: NT_BOOLEAN;
  Time: PLARGE_INTEGER
): NTSTATUS; external ntdll;

