unit SQLDBToolsUnit;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, toolsunit,
  db,
  sqldb, ibconnection, mysql40conn, mysql41conn, mysql50conn, mysql51conn, pqconnection,odbcconn,oracleconnection,sqlite3conn;

type TSQLDBTypes = (mysql40,mysql41,mysql50,mysql51,postgresql,interbase,odbc,oracle,sqlite3);

const MySQLdbTypes = [mysql40,mysql41,mysql50,mysql51];
      DBTypesNames : Array [TSQLDBTypes] of String[19] =
             ('MYSQL40','MYSQL41','MYSQL50','MYSQL51','POSTGRESQL','INTERBASE','ODBC','ORACLE','SQLITE3');
             
      FieldtypeDefinitionsConst : Array [TFieldType] of String[20] =
        (
          '',
          'VARCHAR(10)',
          'SMALLINT',
          'INTEGER',
          '',
          'BOOLEAN',
          'FLOAT',
          '',
          'DECIMAL(18,4)',
          'DATE',
          'TIME',
          'TIMESTAMP',
          '',
          '',
          '',
          'BLOB',
          'BLOB',
          'BLOB',
          '',
          '',
          '',
          '',
          '',
          'CHAR(10)',
          '',
          'BIGINT',
          '',
          '',
          '',
          '',
          '',
          '',
          '',
          '',
          '',
          '',
          'TIMESTAMP',
          'NUMERIC(18,6)',
          '',
          ''
        );
             

type
{ TSQLDBConnector }
  TSQLDBConnector = class(TDBConnector)
  private
    FConnection    : TSQLConnection;
    FTransaction   : TSQLTransaction;
    FQuery         : TSQLQuery;
    FUniDirectional: boolean;
    procedure CreateFConnection;
    procedure CreateFTransaction;
    Function CreateQuery : TSQLQuery;
  protected
    procedure SetTestUniDirectional(const AValue: boolean); override;
    function GetTestUniDirectional: boolean; override;
    procedure CreateNDatasets; override;
    procedure CreateFieldDataset; override;
    procedure DropNDatasets; override;
    procedure DropFieldDataset; override;
    Function InternalGetNDataset(n : integer) : TDataset; override;
    Function InternalGetFieldDataset : TDataSet; override;
    procedure TryDropIfExist(ATableName : String);
  public
    destructor Destroy; override;
    constructor Create; override;
    property Connection : TSQLConnection read FConnection;
    property Transaction : TSQLTransaction read FTransaction;
    property Query : TSQLQuery read FQuery;
  end;

var SQLDbType : TSQLDBTypes;
    FieldtypeDefinitions : Array [TFieldType] of String[20];
    
implementation

{ TSQLDBConnector }

procedure TSQLDBConnector.CreateFConnection;
var i : TSQLDBTypes;
    t : integer;
begin
  for i := low(DBTypesNames) to high(DBTypesNames) do
    if UpperCase(dbconnectorparams) = DBTypesNames[i] then sqldbtype := i;

  FieldtypeDefinitions := FieldtypeDefinitionsConst;
    
  if SQLDbType = MYSQL40 then Fconnection := tMySQL40Connection.Create(nil);
  if SQLDbType = MYSQL41 then Fconnection := tMySQL41Connection.Create(nil);
  if SQLDbType = MYSQL50 then Fconnection := tMySQL50Connection.Create(nil);
  if SQLDbType = MYSQL51 then Fconnection := tMySQL51Connection.Create(nil);
  if SQLDbType in [mysql40,mysql41] then
    begin
    // Mysql versions prior to 5.0.3 removes the trailing spaces on varchar
    // fields on insertion. So to test properly, we have to do the same
    for t := 0 to testValuesCount-1 do
      testStringValues[t] := TrimRight(testStringValues[t]);
    end;
  if SQLDbType in MySQLdbTypes then
    begin
    //MySQL recognizes BOOLEAN, but as synonym for TINYINT, not true sql boolean datatype
    FieldtypeDefinitions[ftBoolean] := '';
    // Use 'DATETIME' for datetime-fields instead of timestamp, because
    // mysql's timestamps are only valid in the range 1970-2038.
    // Downside is that fields defined as 'TIMESTAMP' aren't tested
    FieldtypeDefinitions[ftDateTime] := 'DATETIME';
    FieldtypeDefinitions[ftMemo] := 'TEXT';
    end;
  if SQLDbType = sqlite3 then
    begin
    Fconnection := TSQLite3Connection.Create(nil);
    FieldtypeDefinitions[ftCurrency] := 'CURRENCY';
    FieldtypeDefinitions[ftMemo] := 'CLOB'; //or TEXT SQLite supports both, but CLOB is sql standard (TEXT not)
    FieldtypeDefinitions[ftFixedChar] := '';
    end;
  if SQLDbType = POSTGRESQL then
    begin
    Fconnection := tPQConnection.Create(nil);
    FieldtypeDefinitions[ftCurrency] := 'MONEY';
    FieldtypeDefinitions[ftBlob] := 'BYTEA';
    FieldtypeDefinitions[ftMemo] := 'TEXT';
    FieldtypeDefinitions[ftGraphic] := '';
    end;
  if SQLDbType = INTERBASE then
    begin
    Fconnection := tIBConnection.Create(nil);
    FieldtypeDefinitions[ftBoolean] := '';
    FieldtypeDefinitions[ftMemo] := 'BLOB SUB_TYPE TEXT';
    end;
  if SQLDbType = ODBC then Fconnection := tODBCConnection.Create(nil);
  if SQLDbType = ORACLE then Fconnection := TOracleConnection.Create(nil);

  if SQLDbType in [mysql40,mysql41,mysql50,mysql51,odbc,interbase] then
    begin
    // Some DB's do not support milliseconds in datetime and time fields.
    // Firebird support miliseconds, see BUG 17199 (when resolved, then interbase can be excluded)
    for t := 0 to testValuesCount-1 do
      begin
      testTimeValues[t] := copy(testTimeValues[t],1,8)+'.000';
      testValues[ftTime,t] := copy(testTimeValues[t],1,8)+'.000';
      if length(testValues[ftDateTime,t]) > 19 then
        testValues[ftDateTime,t] := copy(testValues[ftDateTime,t],1,19)+'.000';
      end;
    end;
  if SQLDbType in [postgresql,interbase] then
    begin
    // Some db's do not support times > 24:00:00
    testTimeValues[3]:='13:25:15.000';
    testValues[ftTime,3]:='13:25:15.000';
    if SQLDbType = interbase then
      begin
      // Firebird does not support time = 24:00:00
      testTimeValues[2]:='23:00:00.000';
      testValues[ftTime,2]:='23:00:00.000';
      end;
    end;
  if SQLDbType in [sqlite3] then
    testValues[ftCurrency]:=testValues[ftBCD]; //decimal separator for currencies must be decimal point

  if not assigned(Fconnection) then writeln('Invalid database-type, check if a valid database-type was provided in the file ''database.ini''');

  with Fconnection do
    begin
    DatabaseName := dbname;
    UserName := dbuser;
    Password := dbpassword;
    HostName := dbhostname;
    if length(dbQuoteChars)>1 then
      begin
      FieldNameQuoteChars[0] := dbQuoteChars[1];
      FieldNameQuoteChars[1] := dbQuoteChars[2];
      end;
    open;
    end;
end;

procedure TSQLDBConnector.CreateFTransaction;

begin
  Ftransaction := tsqltransaction.create(nil);
  with Ftransaction do
    database := Fconnection;
end;

Function TSQLDBConnector.CreateQuery : TSQLQuery;

begin
  Result := TSQLQuery.create(nil);
  with Result do
    begin
    database := Fconnection;
    transaction := Ftransaction;
    end;
end;

procedure TSQLDBConnector.SetTestUniDirectional(const AValue: boolean);
begin
  FUniDirectional:=avalue;
  FQuery.UniDirectional:=AValue;
end;

function TSQLDBConnector.GetTestUniDirectional: boolean;
begin
  result := FUniDirectional;
end;

procedure TSQLDBConnector.CreateNDatasets;
var CountID : Integer;
begin
  try
    Ftransaction.StartTransaction;
    TryDropIfExist('FPDEV');
    Fconnection.ExecuteDirect('create table FPDEV (       ' +
                              '  ID INT NOT NULL,           ' +
                              '  NAME VARCHAR(50),          ' +
                              '  PRIMARY KEY (ID)           ' +
                              ')                            ');

    FTransaction.CommitRetaining;

    for countID := 1 to MaxDataSet do
      Fconnection.ExecuteDirect('insert into FPDEV (ID,NAME)' +
                                'values ('+inttostr(countID)+',''TestName'+inttostr(countID)+''')');

    Ftransaction.Commit;
  except
    if Ftransaction.Active then Ftransaction.Rollback
  end;
end;

procedure TSQLDBConnector.CreateFieldDataset;
var CountID : Integer;
    FType   : TFieldType;
    Sql,sql1: String;
begin
  try
    Ftransaction.StartTransaction;
    TryDropIfExist('FPDEV_FIELD');

    Sql := 'create table FPDEV_FIELD (ID INT NOT NULL,';
    for FType := low(TFieldType)to high(TFieldType) do
      if FieldtypeDefinitions[FType]<>'' then
        sql := sql + 'F' + Fieldtypenames[FType] + ' ' +FieldtypeDefinitions[FType]+ ',';
    Sql := Sql + 'PRIMARY KEY (ID))';

    FConnection.ExecuteDirect(Sql);

    FTransaction.CommitRetaining;

    for countID := 0 to testValuesCount-1 do
      begin
      
      Sql :=  'insert into FPDEV_FIELD (ID';
      Sql1 := 'values ('+IntToStr(countID);
      for FType := low(TFieldType)to high(TFieldType) do
        if FieldtypeDefinitions[FType]<>'' then
          begin
          sql := sql + ',F' + Fieldtypenames[FType];
          if testValues[FType,CountID] <> '' then
            sql1 := sql1 + ',' + QuotedStr(testValues[FType,CountID])
          else
            sql1 := sql1 + ',NULL';
          end;
      Sql := sql + ')';
      Sql1 := sql1+ ')';

      Fconnection.ExecuteDirect(sql + ' ' + sql1);
      end;

    Ftransaction.Commit;
  except
    if Ftransaction.Active then Ftransaction.Rollback
  end;
end;

procedure TSQLDBConnector.DropNDatasets;
begin
  if assigned(FTransaction) then
    begin
    try
      if Ftransaction.Active then Ftransaction.Rollback;
      Ftransaction.StartTransaction;
      Fconnection.ExecuteDirect('DROP TABLE FPDEV');
      Ftransaction.Commit;
    Except
      if Ftransaction.Active then Ftransaction.Rollback
    end;
    end;
end;

procedure TSQLDBConnector.DropFieldDataset;
begin
  if assigned(FTransaction) then
    begin
    try
      if Ftransaction.Active then Ftransaction.Rollback;
      Ftransaction.StartTransaction;
      Fconnection.ExecuteDirect('DROP TABLE FPDEV_FIELD');
      Ftransaction.Commit;
    Except
      if Ftransaction.Active then Ftransaction.Rollback
    end;
    end;
end;

function TSQLDBConnector.InternalGetNDataset(n: integer): TDataset;
begin
  Result := CreateQuery;
  with (Result as TSQLQuery) do
    begin
    sql.clear;
    sql.add('SELECT * FROM FPDEV WHERE ID < '+inttostr(n+1));
    UniDirectional:=TestUniDirectional;
    end;
end;

function TSQLDBConnector.InternalGetFieldDataset: TDataSet;
begin
  Result := CreateQuery;
  with (Result as TSQLQuery) do
    begin
    sql.clear;
    sql.add('SELECT * FROM FPDEV_FIELD');
    tsqlquery(Result).UniDirectional:=TestUniDirectional;
    end;
end;

procedure TSQLDBConnector.TryDropIfExist(ATableName: String);
begin
  // This makes live soo much easier, since it avoids the exception if the table already
  // exists. And while this exeption is in a try..except statement, the debugger
  // always shows the exception. Which is pretty annoying
  // It only works with Firebird 2, though.
  try
    if SQLDbType = INTERBASE then
      begin
      FConnection.ExecuteDirect('execute block as begin if (exists (select 1 from rdb$relations where rdb$relation_name=''' + ATableName + ''')) '+
             'then execute statement ''drop table ' + ATAbleName + ';'';end');
      FTransaction.CommitRetaining;
      end;
  except
    FTransaction.RollbackRetaining;
  end;
end;

destructor TSQLDBConnector.Destroy;
begin
  if assigned(FTransaction) then
    begin
    try
      if Ftransaction.Active then Ftransaction.Rollback;
      Ftransaction.StartTransaction;
      Fconnection.ExecuteDirect('DROP TABLE FPDEV2');
      Ftransaction.Commit;
    Except
      if Ftransaction.Active then Ftransaction.Rollback
    end; // try
    end;
  inherited Destroy;

  FreeAndNil(FQuery);
  FreeAndNil(FTransaction);
  FreeAndNil(FConnection);
end;

constructor TSQLDBConnector.Create;
begin
  FConnection := nil;
  CreateFConnection;
  CreateFTransaction;
  FQuery := CreateQuery;
  FConnection.Transaction := FTransaction;
  Inherited;
end;

initialization
  RegisterClass(TSQLDBConnector);
end.

