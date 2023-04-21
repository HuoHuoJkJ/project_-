/* 
 * 本程序是数据中心的公共功能模块，用于从mysql数据入库的操作
 */
#include "_public.h"
#include "_mysql.h"

struct st_arg
{
    char connstr[101];      // 数据库的连接参数，格式：ip,username,password,dbname,port
    char charaset[51];      // 数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况
    char inifilename[301];  // 数据入库的参数配置文件
    char xmlpath[301];      // 待入库xml文件存放的目录
    char xmlpathbak[301];   // xml文件入库后的备份目录
    char xmlpatherr[301];   // 入库失败的xml文件存放的目录
    int  timetvl;           // 本程序运行的时间间隔，本程序常驻内存
    int  timeout;           // 本程序运行时的超时时间
    char pname[51];         // 本程序运行时的程序名
} starg;

#define MAXFIELDCOUNT   100     // 结果集字段的最大数
// #define MAXFIELDLEN     500     // 结果集字段值的最大长度
int     MAXFIELDLEN = -1;       // 结果集字段值的最大长度，可以在_XmlToArg函数中动态的调节大小

char strfieldname[MAXFIELDCOUNT][31];   // 结果集字段名数据，从starg.fieldstr解析得到
int  ifieldlen[MAXFIELDCOUNT];          // 结果集字段的长度数组，从starg.fieldlen解析得到
int  ifieldcount;                       // strfieldname和ifieldlen数组中有效字段的个数
int  incfieldpos = -1;                  // 递增字段在结束集数组中的位置
long imaxincval;                        // 存放递增字段的最大值
char xmlfilename[301];                  // xml文件名

CLogFile        logfile;
connection      conn, conn1;
sqlstatement    stmt;
CPActive        PActive;

void _help();
bool _XmlToArg(char *strxmlbuffer);
void EXIT(int sig);

int main(int argc, char *argv[])
{
    // 写帮助文档
    if (argc != 3) { _help(); return -1; }

    // 处理程序的信号和IO
    CloseIOAndSignal(true);
    signal(SIGINT, EXIT); signal(SIGTERM, EXIT);
    // 在整个程序编写完成且运行稳定后，关闭IO和信号。为了方便调试，暂时不启用CloseIOAndSignal();

    // 日志文件
    if ( logfile.Open(argv[1]) == false )
    { printf("logfile.Open(%s) failed!\n", argv[1]); return -1; }

    // 解析xml字符串，获得参数
    if ( _XmlToArg(argv[2]) == false )
    { logfile.Write("解析xml失败！\n"); return -1; }
    
    // 增加进程的心跳
    // PActive.AddPInfo(starg.timeout, starg.pname);
    PActive.AddPInfo(5000, starg.pname);

    // 连接数据库
    if (conn.connecttodb(starg.connstr, starg.charaset) != 0)
    { logfile.Write("数据库连接失败\n"); return false; }
    logfile.Write("连接数据库%s成功\n", starg.connstr);

    return 0;
}

void _help()
{

    printf("\n");
    printf("Use: xmltodb logfilename xmlbuffer\n\n");

    printf("/project/tools1/bin/procctl 10 /project/tools1/bin/xmltodb /log/idc/xmltodb_vip1.log \"<connstr>127.0.0.1,root,DYT.9525ing,TestDB,3306</connstr><charaset>utf8</charaset><infilename>/project/tools/ini/xmltodb.xml</inifilename><xmlpath>/idcdata/xmltodb/vip1</xmlpath><xmlpathbak>/idcdata/xmltodb/vip1bak</xmlpathbak><xmlpatherr>/idcdata/xmltodb/vip1err</xmlpatherr><timetvl>5</timetvl><timeout>50</timeout><pname>xmltodb_vip1</pname>\"\n\n");

    printf("本程序是通用的功能模块，用于把xml文件入库到MySQL的表中。\n");
    printf("logfilename 是本程序的运行日志文件。\n");
    printf("xmlbuffer   抽取数据源数据，生成xml文件所需参数：\n");
    printf("  connstr         数据库的连接参数，格式：ip,username,password,dbname,port\n");
    printf("  charaset        数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况\n");
    printf("  inifilename     数据入库的参数配置文件\n");
    printf("  xmlpath         待入库xml文件存放的目录\n");
    printf("  xmlpathbak      xml文件入库后的备份目录\n");
    printf("  xmlpatherr      入库失败的xml文件存放的目录\n");
    printf("  timetvl         本程序的时间间隔，单位：秒，视业务需求而定，2-30之间。本程序常驻内存\n");
    printf("  timeout         心跳的超时时间，单位：秒，视xml文件大小而定，建议设置30秒以上，以免程序超时，被守护进程杀掉\n");
    printf("  pname           进程名，建议采用\"xmltodb_后缀\"的方式\n\n\n");
}

bool _XmlToArg(char *strxmlbuffer)
{
    memset(&starg, 0, sizeof(starg));

    GetXMLBuffer(strxmlbuffer, "connstr", starg.connstr, 100);      // 数据库的连接参数，格式：ip,username,password,dbname,port
    if (strlen(starg.connstr) == 0)
    { logfile.Write("connstr值为空！\n"); return false; }

    GetXMLBuffer(strxmlbuffer, "charaset", starg.charaset, 50);     // 数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况
    if (strlen(starg.charaset) == 0)
    { logfile.Write("charaset值为空！\n"); return false; }

    GetXMLBuffer(strxmlbuffer, "inifilename", starg.inifilename, 300); // 数据入库的参数配置文件
    if (strlen(starg.inifilename) == 0)
    { logfile.Write("inifilename值为空！\n"); return false; }

    GetXMLBuffer(strxmlbuffer, "xmlpath", starg.xmlpath, 300);      // 待入库xml文件存放的目录
    if (strlen(starg.xmlpath) == 0)
    { logfile.Write("xmlpath值为空！\n"); return false; }

    GetXMLBuffer(strxmlbuffer, "xmlpathbak", starg.xmlpathbak, 300);// xml文件入库后的备份目录
    if (strlen(starg.xmlpathbak) == 0)
    { logfile.Write("xmlpathbak值为空！\n"); return false; }

    GetXMLBuffer(strxmlbuffer, "xmlpatherr", starg.xmlpatherr, 300);// 入库失败的xml文件存放的目录
    if (strlen(starg.xmlpatherr) == 0)
    { logfile.Write("xmlpatherr值为空！\n"); return false; }

    GetXMLBuffer(strxmlbuffer, "timetvl", &starg.timetvl);          // 本程序的时间间隔，单位：秒，视业务需求而定，2-30之间。本程序常驻内存
    if (starg.timetvl <= 2 && starg.timetvl >= 30)
    { logfile.Write("timetvl的值不规范，已改为30！\n"); starg.timetvl = 30; }

    GetXMLBuffer(strxmlbuffer, "timeout", &starg.timeout);          // 进程心跳的超时时间
    if (starg.timeout == 0)
    { logfile.Write("timeout值为空！\n"); return false; }

    GetXMLBuffer(strxmlbuffer, "pname", starg.pname, 50);           // 进程名
    if (strlen(starg.pname) == 0)
    { logfile.Write("pname值为空！\n"); return false; }
    
    CCmdStr CmdStr;

    // 把starg.fieldlen解析到ifieldlen数组中
    CmdStr.SplitToCmd(starg.fieldlen, ",");
    if ((ifieldcount = CmdStr.CmdCount()) > MAXFIELDCOUNT)
    { logfile.Write("fieldlen的字段数太多，超出了最大限制%d\n", MAXFIELDCOUNT); return false; }
    
    for (int ii = 0; ii < ifieldcount; ii++)
    {
        CmdStr.GetValue(ii, &ifieldlen[ii]);
        // if (ifieldlen[ii] > MAXFIELDLEN) ifieldlen[ii] = MAXFIELDLEN;
        if (ifieldlen[ii] > MAXFIELDLEN) MAXFIELDLEN = ifieldlen[ii];
    }

    // 把starg.fieldstr解析到strfieldname数组中
    CmdStr.SplitToCmd(starg.fieldstr, ",");
    if (CmdStr.CmdCount() > MAXFIELDCOUNT)
    { logfile.Write("fieldstr的字段数太多，超出了最大限制%d\n", MAXFIELDCOUNT); return false; }

    for (int ii = 0; ii < CmdStr.CmdCount(); ii++)
        CmdStr.GetValue(ii, strfieldname[ii], 30);

    // 判断strfieldname和ifieldlen的元素数量是否相等
    if (ifieldcount != CmdStr.CmdCount())
    { logfile.Write("strfieldname和ifieldlen的元素数量不相等\n"); return false; }

    // 查找自增字段在结果集中的位置
    if (strlen(starg.incfield) != 0)
    {
        // logfile.Write("incfield = %s\n", starg.incfield);
        for (int ii = 0; ii < ifieldcount; ii++)
            if (strcmp(strfieldname[ii], starg.incfield) == 0) { incfieldpos = ii; break; }
        if (incfieldpos == -1)
        { logfile.Write("递增字段%s不在列表%s中\n", starg.incfield, starg.fieldstr); return false; }
        
        // 如果自增字段存在，我们需要判断文件或者数据库表连接字符串是否为空
        if ((strlen(starg.incfieldname)==0) && (strlen(starg.connstr1)==0))
        { logfile.Write("自增字段存放的文件路径%s 和 自增字段存放的数据库表连接字符串%s均为空\n"); return false; }
    }
    return true;
}

bool _InStarttime()
{
    // 进程运行的时间区间，例如02、13，如果进程启动时，时间刚好为02或13时，则运行，其他时间不运行
    if (starg.starttime != 0)
    {
        // 获取当前时间
        char HH24[3];   memset(HH24, 0, sizeof(HH24));
        LocalTime(HH24, "hh24");

        // 判断starttime数组中的值是否包含当前时间
        if (strstr(HH24, starg.starttime) == 0)
            return false;
    }

    return true;
}

bool _dminingmysql()
{
    // 读取starg.incfieldname文件或starg.connstr1连接的数据库中的incfield的值
    _ReadIncfield();

    // 准备sql语句 绑定变量
    stmt.connect(&conn);
    stmt.prepare(starg.selectsql);
    // 绑定输出变量
    char strfieldvalue[ifieldcount][MAXFIELDLEN+1];
    // logfile.Write("incfieldpos = %d, ifieldcount = %d\n", incfieldpos, ifieldcount);
    for (int ii = 1; ii <= ifieldcount; ii++)
        stmt.bindout(ii, strfieldvalue[ii-1], ifieldlen[ii-1]);
    // 绑定keyid的值
    if (strlen(starg.incfield) != 0) stmt.bindin(1, &imaxincval);

    // 执行sql语句
    if (stmt.execute() != 0)
    { logfile.Write("stmt.execute() 失败\n%s\n%s\n", stmt.m_sql, stmt.m_cda.message); return false; }

    PActive.UptATime();

    CFile File;

    while (true)
    {
        memset(strfieldvalue, 0, sizeof(strfieldvalue));
        
        // 从结果集(缓冲区)中取出数据 从结果集中获取一条记录。
        if (stmt.next() != 0) break;

        PActive.UptATime();

        // 判断文件是否已经打开，在判断结果集是否为空下面打开文件，避免出现空文件的情况
        if (File.IsOpened() == false)
        {
            // 拼接xml文件名
            _CreatXMLName();
            if (File.OpenForRename(xmlfilename, "w+") == false)
            { logfile.Write("打开文件%s失败\n", xmlfilename); return false; }
            File.Fprintf("<data>\n");
        }

        for (int ii = 1; ii <= ifieldcount; ii++)
            File.Fprintf("<%s>%s</%s>", strfieldname[ii-1], strfieldvalue[ii-1], strfieldname[ii-1]);
        File.Fprintf("</endl>\n");

        if ((starg.maxcount>0) && (stmt.m_cda.rpc % starg.maxcount == 0))
        {
            File.Fprintf("</data>");
            if (File.CloseAndRename() == false)
            { logfile.Write("\n"); return false; }
            logfile.Write("生成文件%s(%d行)成功\n", xmlfilename, starg.maxcount);
        }
        // 比较keyid的大小，取最大值
        if ((strlen(starg.incfield) != 0) && imaxincval < atol(strfieldvalue[incfieldpos]) )
        imaxincval = atol(strfieldvalue[incfieldpos]);

        PActive.UptATime();
    }
    if (File.IsOpened() == true)
    {
        File.Fprintf("</data>");
        if (File.CloseAndRename() == false)
        { logfile.Write("\n"); return false; }
        if (starg.maxcount == 0)
            logfile.Write("生成文件%s(%d行)成功\n", xmlfilename, stmt.m_cda.rpc);
        else
            logfile.Write("生成文件%s(%d行)成功\n", xmlfilename, stmt.m_cda.rpc%starg.maxcount);
    }
    
    // 将keyid的值写入incfieldname文件中，如果结果集为空，肯定不需要更新keyid
    if (stmt.m_cda.rpc > 0) _WriteIncfield();

    return true;
}

bool _ReadIncfield()
{
    imaxincval = 0;
    // 判断incfield的值是否为空，如果为空说明不需要增量抽取
    if (strlen(starg.incfield) == 0) return true;

    // 从数据库表中获取imaxincval的值
    if (strlen(starg.connstr1) != 0)
    {
        // 准备查询的sql语句
        stmt.connect(&conn1);
        stmt.prepare("select maxincvalue from T_MAXINCVALUE where pname=:1");
        stmt.bindin(1, starg.pname, 50);
        stmt.bindout(1, &imaxincval);
        // 这里不需要对sql语句执行的结果进行判断，原因与下面打开文件一样
        stmt.execute();
        stmt.next();
    }
    // 从文件中获取imaxincval的值
    else
    {
        CFile File;
        // 打开失败的原因：可能是第一次打开，可能该文件被清理掉了
        if (File.Open(starg.incfieldname, "r") == false) return true;
        
        char strtemp[31]; memset(strtemp, 0, sizeof(strtemp));
        // 读取内容
        File.Fgets(strtemp, 30);
        // logfile.Write("strtemp = %s | %ld\n", strtemp, atol(strtemp));

        // 赋值给imaxincval
        imaxincval = atol(strtemp);
    }

    logfile.Write("上次已抽取数据的位置(%s)：%ld\n", starg.incfield, imaxincval);

    return true;
}

void _CreatXMLName()
{
    // xml文件名格式：starg.outpath + starg.bfilename + strlocaltime + starg.efilename
    char strlocaltime[21];      memset(strlocaltime, 0, sizeof(strlocaltime));
    LocalTime(strlocaltime, "yyyymmddhh24miss");
    static int iseq = 1;    // 设置静态变量，防止同一秒内将本应分割的数据写入同一文件中
    
    SNPRINTF(xmlfilename, 300, sizeof(xmlfilename), "%s/%s_%s_%s_%d.xml", starg.outpath, starg.bfilename, strlocaltime, starg.efilename, iseq++);
}

bool _WriteIncfield()
{
    // 判断incfield的值是否为空，如果为空说明不需要增量抽取
    if (strlen(starg.incfield) == 0) return true;

    if (strlen(starg.connstr1) != 0)
    {
        // 准备更新的sql语句
        stmt.connect(&conn1);
        stmt.prepare("update T_MAXINCVALUE set maxincvalue=:1 where pname=:2");
        if (stmt.m_cda.rc == 1146)
        {
            conn1.execute("create table T_MAXINCVALUE(pname varchar(50),maxincvalue numeric(15),primary key(pname))");
            conn1.execute("insert into T_MAXINCVALUE values('%s',%ld)", starg.pname, imaxincval);
            conn1.commit();
            return true;
        }
        stmt.bindin(1, &imaxincval);
        stmt.bindin(2, starg.pname, 50);
        if (stmt.execute() != 0)
        { logfile.Write("stmt.execute() 失败\n%s\n%s\n", stmt.m_sql, stmt.m_cda.message); return false; }
        conn1.commit();
    }
    else
    {
        CFile File;
        // 打开文件
        if (File.Open(starg.incfieldname, "w+") == false)
        { logfile.Write("打开文件%s失败\n", starg.incfieldname); return false; }

        // 写入内容
        File.Fprintf("%ld", imaxincval);
    }

    return true;
}

void EXIT(int sig)
{
    printf("接收到%s信号，进程退出\n\n", sig);
    exit(0);
}