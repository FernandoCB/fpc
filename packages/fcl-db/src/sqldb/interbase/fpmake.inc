{ Make DB directory and all subdirectories }
{ FULLSQLDB will be defined if included from SQLDB directory alone}
{ FULLDB will be defined if included from db directory alone}
{ FULLFCL will be defined if included from main FCL directory }

Targets.ResetDefaults;
Targets.DefaultDir:='db/sqldb/interbase';
{ Drivers only for the following OSes }
Targets.DefaultOS:=[win32,openbsd,netbsd,freebsd,darwin,linux];
T:=Targets.AddUnit('ibconnection');
T.ResourceStrings:=True;