/*================================================================
 *   Copyright (C) 2021 hqyj Ltd. All rights reserved.
 *   
 *   文件名称：员工管理系统.c
 *   创 建 者：liu
 *   创建日期：2021年06月25日
 *   描    述：服务器
 *
 ================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define N 256
#define T 20
#define M 40
#define P 12

#define IP "192.168.0.135"
#define PORT 8887

#define REGIST 'R'
#define LOGIN 'L'
#define ADD 'A'
#define DELETE 'D'
#define MODIFY 'M'
#define SEAR 'S'
#define ATTENDANCE 'T'
#define QUIT 'Q'
#define HIST 'H'
#define CAN 'C'
#define SIGNIN 'I' //打卡签到

#define NAME "NA"
#define PSWD "PW"
#define AGE "AG"
#define NUM "NO"
#define LEVEL "LV"
#define SALARY "SA"
#define DEPART "DP"
#define ADDRESS "AD"
#define PHONE "PH"
//登陆
#define ADMIN "AM" //管理员
#define USER "US"  //普通用户

//查询
#define ALLINFO "AI"       //查全员信息
#define PRIVATEINFO "PI"   //查个人信息
#define ALLATTEND "AT"     //查全员考勤
#define PRIVATEATTEND "PT" //查个人考勤
#define ERR_LOG(msg)                                            \
      do                                                        \
      {                                                         \
            perror(msg);                                        \
            printf("%d %s %s\n", __LINE__, __func__, __FILE__); \
            return -1;                                          \
      } while (0)
typedef struct
{ //定义通信结构体
      //      char ID;
      char type;
      char type_sub[3];
      char name[T];
      char pswd[T];
      int age;
      int NO;
      int level;
      int salary;
      char depart[T];
      char addr[M];
      char pho[P];
      char text[T]; //用来存服务器反馈的信息  //可传输考勤信息
} MSG;

typedef void (*sighandler_t)(int); //void (*)(int);  --->sighandler_t

int do_regist(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db);
//登陆
int do_login(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db);
//添加员工
int do_add_stf(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db);
//删除员工
int do_del_stf(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db);
//修改员工
int do_mod_stf(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db);
//查询员工信息 和考勤信息
int do_search(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db);
//考勤记录
int show_attendance(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db);
//导入打卡记录
int do_sign_in(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db); //查询人员信息 含用户名 和密码信息
//退出
int do_quit(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db);
int find_free_num(sqlite3 *db);
void handler(int sig)
{
      while (waitpid(-1, NULL, WNOHANG) > 0)
            ;
      printf("回收子进程成功\n");
}

int main(int argc, char *argv[])
{
      sighandler_t s = signal(SIGCHLD, handler);
      if (s == SIG_ERR)
            ERR_LOG("signal");

      //打开员工信息数据库
      //sqlite3 *db = NULL;
      sqlite3 *db;
      if (sqlite3_open("./staff.db", &db) != SQLITE_OK)
      {
            //打印错误信息
            fprintf(stderr, "%s\n", sqlite3_errmsg(db));
            return -1;
      }
      printf("员工数据库打开成功\n");

      //创建员工信息表格
      char *errmsg = NULL;
      char sql[N] = "create table if not exists staff_info(name char primary key,age int,NO int, level int, salary int, depart char, addr char, phone char);";

      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            return -1;
      }
      printf("员工信息表格创建成功\n");

      //创建人员账号密码表
      char sqlacn[N] = "create table if not exists usrinfo(name char primary key,pswd char,ID char);";
      //char *errmsg = NULL;                                //name 为主键
      if (sqlite3_exec(db, sqlacn, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            return -1;
      }
      printf("普通员工帐号密码表创建成功\n");

      //创建考勤信息记录表
      char sqlatd[N] = "create table if not exists attendance(name char,time char);";
      if (sqlite3_exec(db, sqlatd, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            return -1;
      }
      printf("考勤信息记录表创建成功\n");

      //1.创建socket
      int sfd = socket(AF_INET, SOCK_STREAM, 0);
      if (sfd < 0)
            ERR_LOG("socket");

      //端口快速重用
      int reuse = 1;
      if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
            ERR_LOG("setsockopt");

      //2.绑定服务器IP和端口号
      struct sockaddr_in sin;
      sin.sin_family = AF_INET;
      sin.sin_port = htons(PORT);
      sin.sin_addr.s_addr = inet_addr(IP);

      if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
            ERR_LOG("bind");

      //3.将套接字设为监听状态
      if (listen(sfd, 5) < 0)
            ERR_LOG("listen");
      printf("监听套接字成功\n");

      //4.获得连接后的套接字
      struct sockaddr_in cin;
      socklen_t clen = sizeof(cin);
      int newsfd = 0;

      //	MSG usr = {'0',"","",""};//用来接用户信息
      while (1)
      {
            //创建连接后的套接字
            newsfd = accept(sfd, (struct sockaddr *)&cin, &clen);
            printf("newsfd = %d\n", newsfd); //OK
            pid_t pid = fork();

            if (pid > 0)
            {
                  //父进程
                  close(newsfd);
            }
            else if (0 == pid)
            {
                  //子进程
                  close(sfd);
                  MSG usr; //用来接用户信息
                  memset(&usr, 0, sizeof(usr));
                  int res = 0;
                  while (1)
                  {
                        memset(&usr, 0, sizeof(usr));
                        printf("返回阻塞点，等待重新输入\n");
                        if ((res = recv(newsfd, &usr, sizeof(usr), 0)) < 0)
                              ERR_LOG("recv");
                        printf("%c,%s,%s\n", usr.type, usr.name, usr.pswd);

                        if (res == 0)
                        {
                              printf("客户端关闭\n");
                              break;
                        }

                        switch (usr.type)
                        {
                        case REGIST:
                              printf("usr.type = %c\n", usr.type);
                              do_regist(newsfd, usr, cin, db);
                              break;
                        case LOGIN:
                              printf("usr.type = %c\n", usr.type);
                              do_login(newsfd, usr, cin, db);
                              break;
                        case ADD:
                              printf("usr.type = %c\n", usr.type);
                              do_add_stf(newsfd, usr, cin, db);
                              break;
                        case DELETE:
                              printf("usr.type = %c\n", usr.type);
                              do_del_stf(newsfd, usr, cin, db);
                              break;
                        case MODIFY:
                              printf("usr.type = %c\n", usr.type);
                              do_mod_stf(newsfd, usr, cin, db);
                              break;
                        case SEAR:
                              printf("usr.type = %c\n", usr.type);
                              do_search(newsfd, usr, cin, db);
                              break;
                        case SIGNIN:
                              printf("usr.type = %c\n", usr.type);
                              do_sign_in(newsfd, usr, cin, db);
                              break;
                        case QUIT:
                              printf("usr.type = %c\n", usr.type);
                              do_quit(newsfd, usr, cin, db);
                              break;
                        default:
                              //goto END;
                              break;
                        }
                  }
                  printf("跳出while\n");
                  close(newsfd);
                  exit(1);
            }
      }
END:
      //关闭数据库
      sqlite3_close(db);

      return 0;
}

//注册
int do_regist(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db)
{

      //注册 账号存在则注册失败 usrinfo表   同时 在staff_info表 生成默认员工信息
      char sql[N] = "";
      printf("%s,%s\n", usr.name, usr.pswd);
      sprintf(sql, "insert into usrinfo values('%s','%s','N');", usr.name, usr.pswd); // 账号密码表中 增加ID位  N  或 V  只能注册普通用户
      printf("sql = %s %d\n", sql, __LINE__);
      char *errmsg = NULL;
      printf("%c,%s,%s,%s\n", usr.type, usr.name, usr.pswd, usr.text);
      bzero(usr.text, sizeof(usr.text));
      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            strcpy(usr.text, "注册失败");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
            {
                  printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
                  ERR_LOG("send");
            }
      }
      printf("sqlite3_exec: %d\n", __LINE__); //

      //同时 在staff_info表 生成默认员工信息
      int staff_num;
      staff_num = find_free_num(db); //获取未被占用的工号NO
      printf("staff_num = %d\n", staff_num);

      bzero(sql, sizeof(sql)); //工号生成

      sprintf(sql, "insert into staff_info values('%s',21,%d,1,200,'free','free','00000000000');", usr.name, staff_num); //staff_info 增加ID位  N  或 V  只能注册普通用户
      printf("sql = %s\n", sql);
      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            sprintf(sql, "delete from usrinfo where name='%s'", usr.name); //删除原来创建的账户
            if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
            {
                  strcpy(usr.text, "注册失败");
                  if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  {
                        printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
                        ERR_LOG("send");
                  }
            }
            strcpy(usr.text, "注册失败");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
            {
                  printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
                  ERR_LOG("send");
            }
      }
      //无错误 返回"注册成功"
      strcpy(usr.text, "注册成功");
      if (send(newsfd, &usr, sizeof(usr), 0) < 0)
            ERR_LOG("send");
      printf("%c,%s,%s,%s\n", usr.type, usr.name, usr.pswd, usr.text);

      return 0;
}

//登陆
int do_login(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db)
{
      printf("%c,%s,%s,%s\n", usr.type, usr.name, usr.pswd, usr.text);
      char sql[N] = "";
      char *errmsg = NULL;

      sprintf(sql, "select * from usrinfo where name='%s' and pswd='%s';", usr.name, usr.pswd);
      //printf("%c, %s，%s, %s, %s\n", usr.type, usr.type_sub, usr.name, usr.pswd, usr.text);
      printf("sql = %s %d\n", sql, __LINE__);
      char **pres = NULL;
      int row, column;
      if (sqlite3_get_table(db, sql, &pres, &row, &column, &errmsg) != 0)
      {
            printf("sqlite3_get_table:%s %d\n", errmsg, __LINE__);
            return -1;
      }

      int line = 0, list = 0, j = 0;
      //row中不包含字段名的那一行，但实际结果中有包含，所以要将字段名那一行加上
      //所以循环条件row+1;
      int flag = 0;
      int ret = -1;

      for (line = 1; line < row + 1; line++)
      {
            //账号 密码匹配成功
            if (strcmp(pres[line * column], usr.name) == 0 && strcmp(pres[line * column + 1], usr.pswd) == 0)
            {
                  if (strncmp("V", pres[line * column + column - 1], 1) == 0) //ID匹配 确认是管理员还是普通用户
                  {
                        //登陆成功
                        flag++;
                        strcpy(usr.text, "admin");
                        printf("%s\n", usr.text);
                        ret = send(newsfd, &usr, sizeof(usr), 0);
                        printf("ret = %d\n", ret);
                        if (ret < 0)
                              ERR_LOG("send");
                        printf("管理员 登陆成功\n");
                        break;
                  }
                  else
                  {
                        //登陆成功
                        flag++;
                        strcpy(usr.text, "normal");
                        if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                              ERR_LOG("send");
                        printf("普通用户 登陆成功\n");
                        break;
                  }
            }
      }
      if (flag == 0)
      {
            strcpy(usr.text, "ERROR\n");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
            printf("%s\n", usr.text);
      }
      /* END:
{
      sqlite3_free_table(pres);
      return 0;
} */
      printf("do_login over\n");
      sleep(1);
      sqlite3_free_table(pres);
      return 0;
}
//添加员工
int do_add_stf(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db)
{
      //工号自动获取
      int staff_num = find_free_num(db);
      printf("do_add_stf\n");
      char sql[N] = "";
      //将 员工信息  导入到员工信息表中
      //20         4       4           4       4         20        40       20
      sprintf(sql, "insert into staff_info values('%s',%d,%d,%d,%d,'%s','%s','%s');", usr.name, usr.age, staff_num, usr.level, usr.salary, usr.depart, usr.pho, usr.addr);
      printf("sql = %s %d\n", sql, __LINE__);
      char *errmsg = NULL;

      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            memset(&usr, 0, sizeof(usr));
            strcpy(usr.text, "add error");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
            printf("add error\n");
            return -1;
      }
      //同时 生成账号密码信息  usrinfo表
      printf("%c,%s,%s,%s\n", usr.type, usr.name, usr.pswd, usr.text);

      sprintf(sql, "insert into usrinfo values('%s','666666','N');", usr.name); //默认密码为666666 普通用户
      //printf("%c, %s，%s, %s, %s\n", usr.type, usr.type_sub, usr.name, usr.pswd, usr.text);
      printf("sql = %s %d\n", sql, __LINE__);
      char **pres = NULL;
      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            memset(&usr, 0, sizeof(usr));
            strcpy(usr.text, "add error");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
            printf("add error\n");
            return -1;
      }

      //导入成功
      memset(&usr, 0, sizeof(usr));
      strcpy(usr.text, "add successfully");
      if (send(newsfd, &usr, sizeof(usr), 0) < 0)
            ERR_LOG("send");
      printf("insert successfully\n");

      return 0;
}

int find_free_num(sqlite3 *db)
{
      //按工号排序
      char sql[N] = "";
      char *errmsg = NULL;
      sprintf(sql, "select * from staff_info order by NO asc;");
      char **pres = NULL;
      int row, column, list, i;
      if (sqlite3_get_table(db, sql, &pres, &row, &column, &errmsg) != 0)
      {
            printf("sqlite3_get_table:%s %d\n", errmsg, __LINE__);
            return -1;
      }
      int staff_num = 1; //4      8
      printf("row = %d, column = %d\n", row, column);
      printf("zzzzz\n");
      for (list = 1; list < row + 1; list++)
      {
            printf("pres[%d] = %s\n", list * column + 2, pres[list * column + 2]);
            if (pres[list * column + 2] == NULL)
                  return 0;
            else if (list == atoi(pres[list * column + 2]))
            {
                  continue;
            }
            else
            {
                  staff_num = list;
                  break;
            }
      }
      staff_num = list;
      return staff_num;
}
//删除员工
int do_del_stf(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db)
{
      printf("do_del_stf\n");
      //将 员工信息  从员工信息表中删除
      char sql[N] = "";
      sprintf(sql, "delete from staff_info where name='%s' and NO=%d;", usr.name, usr.NO);
      printf("sql = %s %d\n", sql, __LINE__);
      char *errmsg = NULL;

      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            memset(&usr, 0, sizeof(usr));
            strcpy(usr.text, "del_ops error");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
            printf("del_ops error\n");
            return -1;
      }
      //同时删除 账号密码表
      sprintf(sql, "delete from usrinfo where name='%s';", usr.name);
      printf("sql = %s %d\n", sql, __LINE__);
      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            memset(&usr, 0, sizeof(usr));
            strcpy(usr.text, "del_ops error");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
            printf("del_ops error\n");
            return -1;
      }
      //导入成功
      memset(&usr.text, 0, sizeof(usr.text));
      strcpy(usr.text, "del_ops successful");
      if (send(newsfd, &usr, sizeof(usr), 0) < 0)
            ERR_LOG("send");
      printf("del_ops successful\n");
      return 0;
}
//修改员工
int do_mod_stf(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db)
{
      printf("do_mod_stf\n");
      //将 员工信息  从员工信息表中删除
      char sql[N] = "";
      char sqln[N] = "";

      char *errmsg = NULL;
      if (strcmp(usr.type_sub, NAME) == 0)
      {
            printf("修改姓名\n");
            sprintf(sql, "update staff_info set name='%s' where NO=%d;", usr.name, usr.NO);
      }
      else if (strcmp(usr.type_sub, PSWD) == 0)
      {
            printf("修改密码\n");
            sprintf(sql, "update usrinfo set pswd='%s' where name='%s';", usr.pswd, usr.name);
      }
      else if (strcmp(usr.type_sub, AGE) == 0)
      {
            printf("修改年龄\n");
            if (strcmp(usr.text, "admin") == 0)
                  sprintf(sql, "update staff_info set age=%d where NO=%d;", usr.age, usr.NO);
            else
                  sprintf(sql, "update staff_info set age=%d where name='%s';", usr.age, usr.name);
      }
      else if (strcmp(usr.type_sub, NUM) == 0)
      {
            printf("修改工号\n");
            sprintf(sql, "update staff_info set NO=%d where name='%s' and phone='%s';", usr.NO, usr.name, usr.pho);
      }
      else if (strcmp(usr.type_sub, LEVEL) == 0)
      {
            printf("修改职级\n");
            if (strcmp(usr.text, "admin") == 0)
                  sprintf(sql, "update staff_info set level=%d where NO=%d;", usr.level, usr.NO);
            else
                  sprintf(sql, "update staff_info set level=%d where name='%s';", usr.age, usr.name);
      }
      else if (strcmp(usr.type_sub, SALARY) == 0)
      {
            printf("修改薪资\n");
            if (strcmp(usr.text, "admin") == 0)
                  sprintf(sql, "update staff_info set salary=%d where NO=%d;", usr.salary, usr.NO);
            else
                  sprintf(sql, "update staff_info set salary=%d where name='%s';", usr.salary, usr.name);
      }
      else if (strcmp(usr.type_sub, DEPART) == 0)
      {
            printf("修改部门\n");
            if (strcmp(usr.text, "admin") == 0)
                  sprintf(sql, "update staff_info set depart='%s' where NO=%d;", usr.depart, usr.NO);
            else
                  sprintf(sql, "update staff_info set depart='%s' where name='%s';", usr.depart, usr.name);
      }
      else if (strcmp(usr.type_sub, ADDRESS) == 0)
      {
            printf("修改地址\n");
            if (strcmp(usr.text, "admin") == 0)
                  sprintf(sql, "update staff_info set addr='%s' where NO=%d;", usr.addr, usr.NO);
            else
                  sprintf(sql, "update staff_info set addr='%s' where name='%s';", usr.addr, usr.name);
      }
      else if (strcmp(usr.type_sub, PHONE) == 0)
      {
            printf("修改电话\n");
            if (strcmp(usr.text, "admin") == 0)
                  sprintf(sql, "update staff_info set phone='%s' where NO=%d  ;", usr.pho, usr.NO);
            else
                  sprintf(sql, "update staff_info set phone='%s' where name='%s';", usr.pho, usr.name);
      }
      printf("sql = %s %d\n", sql, __LINE__);
      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            memset(&usr, 0, sizeof(usr));
            strcpy(usr.text, "modify_ops error");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
            printf("modify_ops error\n");
            return -1;
      }
      //导入成功
      memset(&usr.text, 0, sizeof(usr.text));
      strcpy(usr.text, "mod_ops successful");
      if (send(newsfd, &usr, sizeof(usr), 0) < 0)
            ERR_LOG("send");
      printf("mod_ops successful\n");
      return 0;
}

//查询员工
int do_search(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db) //查询单词
{
      char sql[N] = "";
      char *errmsg = NULL;
      if (strcmp(usr.type_sub, PRIVATEINFO) == 0 || strcmp(usr.type_sub, ALLINFO) == 0)
      {
            printf("%c,%s,%s,%s,%d,%d,%d,%d,%s,%s,%s,%s,%d\n", usr.type, usr.type_sub, usr.name, usr.pswd, usr.age,
                   usr.NO, usr.level, usr.salary, usr.depart, usr.addr, usr.pho, usr.text, __LINE__);
      }
      else
            printf("%s, %s\n", usr.name, usr.text); //传输考勤信息
      if (strcmp(usr.type_sub, PRIVATEINFO) == 0)
      {
            sprintf(sql, "select * from staff_info where name='%s';", usr.name);
      }
      if (strcmp(usr.type_sub, ALLINFO) == 0)
      {
            sprintf(sql, "select * from staff_info;");
      }
      if (strcmp(usr.type_sub, PRIVATEATTEND) == 0)
      {
            sprintf(sql, "select * from attendance where name='%s';", usr.name);
      }
      if (strcmp(usr.type_sub, ALLATTEND) == 0)
      {
            sprintf(sql, "select * from attendance;");
      }

      printf("sql = %s %d\n", sql, __LINE__);
      char **pres = NULL;
      int row, column;
      //行     列
      if (sqlite3_get_table(db, sql, &pres, &row, &column, &errmsg) != 0)
      {
            printf("sqlite3_get_table:%s %d\n", errmsg, __LINE__);
            return -1;
      }
      int flag = 0;
      int line;
      int list;
      for (line = 1; line < row + 1; line++)
      {
            for (list = 0; list < column; list++)
            {
                  /*  if (strcmp(pres[line * column], usr.name) == 0 || (strcmp(usr.type_sub, ALLINFO) == 0 || strcmp(usr.type_sub, ALLATTEND) == 0)) //printf("第line(1~row+1)行读取\n");
                  { */

                  /* memset(&usr, 0, sizeof(usr));
                        usr.type = SEAR; //恢复指令位 */
                  if (strcmp(usr.type_sub, PRIVATEINFO) == 0 || strcmp(usr.type_sub, ALLINFO) == 0)
                  {
                        if (list == 0)
                        {
                              strcpy(usr.name, pres[line * column + list]);
                              printf("spres[%d] = %s\n", (line * column + list), pres[line * column + list]);
                        }
                        else if (list == 1)
                        {
                              usr.age = atoi(pres[line * column + list]);
                              printf("usr.age = %d\n", usr.age);
                        }
                        else if (list == 2)
                        {
                              usr.NO = atoi(pres[line * column + list]);
                              printf("usr.NO = %d\n", usr.NO);
                        }
                        else if (list == 3)
                        {
                              usr.level = atoi(pres[line * column + list]);
                              printf("usr.level = %d\n", usr.level);
                        }
                        else if (list == 4)
                        {
                              usr.salary = atoi(pres[line * column + list]);
                              printf("usr.salary = %d\n", usr.salary);
                        }
                        else if (list == 5)
                        {
                              strcpy(usr.depart, pres[line * column + list]);
                              printf("spres[%d] = %s\n", (line * column + list), pres[line * column + list]);
                        }
                        else if (list == 6)
                        {
                              strcpy(usr.addr, pres[line * column + list]);
                              printf("spres[%d] = %s\n", (line * column + list), pres[line * column + list]);
                        }
                        else if (list == 7)
                        {
                              strcpy(usr.pho, pres[line * column + list]);
                              printf("spres[%d] = %s\n", (line * column + list), pres[line * column + list]);
                        }
                  }
                  else if (strcmp(usr.type_sub, PRIVATEATTEND) == 0 || strcmp(usr.type_sub, ALLATTEND) == 0)
                  {
                        if (list == 0) //第 0列
                        {
                              strcpy(usr.name, pres[line * column + list]);
                              printf("spres[%d] = %s\n", (line * column + list), pres[line * column + list]);
                        }
                        else //第1列
                        {
                              strcpy(usr.text, pres[line * column + list]);
                              printf("spres[%d] = %s\n", (line * column + list), pres[line * column + list]);
                        }
                  }
                  /* } */
            }
            //查找成功
            flag++;
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
            if (strcmp(usr.type_sub, PRIVATEINFO) == 0 || strcmp(usr.type_sub, ALLINFO) == 0)
            {
                  printf("%c,%s,%s,%s,%d,%d,%d,%d,%s,%s,%s,%s,%d\n", usr.type, usr.type_sub, usr.name, usr.pswd, usr.age,
                         usr.NO, usr.level, usr.salary, usr.depart, usr.addr, usr.pho, usr.text, __LINE__);
            }
            else
                  printf("%s, %s\n", usr.name, usr.text); //传输考勤信息
      }
      if (flag != 0)
      {
            strcpy(usr.text, "ransmit over");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
            printf("ransmit over\n");
      }
      if (flag == 0)
      {
            bzero(usr.text, T);
            strcpy(usr.text, "not found");
            if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                  ERR_LOG("send");
      }
      sqlite3_free_table(pres);

      return 0;
}
//导入打卡记录
int do_sign_in(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db)
{
      //将 打卡时间  导入到历史记录表中

      time_t t;
      struct tm *tm = NULL;
      t = time(NULL);
      tm = localtime(&t);
      char sql[N] = "";
      char time[M] = "";
      sprintf(time, "%d-%d-%d %d:%d:%d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
      putchar(10);
      sprintf(sql, "insert into attendance values('%s','%s');", usr.name, time);
      printf("sql = %s %d", sql, __LINE__);
      char *errmsg = NULL;
      if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != 0)
      {
            printf("sqlite3_exec:%s %d\n", errmsg, __LINE__);
            return -1;
      }
      //成功
      strcpy(usr.text, time);
      if (send(newsfd, &usr, sizeof(usr), 0) < 0)
            ERR_LOG("send");
      printf("%s\n", usr.text);
      printf("签到时间回发成功\n");

      return 0;
}
//考勤记录
int show_attendance(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db) //历史记录
{
      printf("%c,%s,%s,%s\n", usr.type, usr.name, usr.pswd, usr.text);

      char sql[N] = "";
      char *errmsg = NULL;
      sprintf(sql, "select * from histroy where name='%s';", usr.name);
      char **pres = NULL;
      int row, column;

      if (sqlite3_get_table(db, sql, &pres, &row, &column, &errmsg) != 0)
      {
            printf("sqlite3_get_table:%s %d\n", errmsg, __LINE__);
            return -1;
      }

      int line = 0, list = 0, i = 0;
      //row中不包含字段名的那一行，但实际结果中有包含，所以要将字段名那一行加上
      //所以循环条件row+1;
      for (line = 0; line < row + 1; line++)
      {
            for (i = 0; i < 4; i++)
            {
                  printf("%s\t", pres[line * column + i]);
                  bzero(usr.text, T);
                  strcpy(usr.text, pres[line * column + i]);
                  if (send(newsfd, &usr, sizeof(usr), 0) < 0)
                        ERR_LOG("send");
            }
      }
      bzero(usr.text, T);
      strcpy(usr.text, "histroy success");
      if (send(newsfd, &usr, sizeof(usr), 0) < 0)
            ERR_LOG("send");
      printf("%s\n", usr.text);
      sleep(1);

      sqlite3_free_table(pres);
      return 0;
}

//退出
int do_quit(int newsfd, MSG usr, struct sockaddr_in cin, sqlite3 *db)
{
      printf("do_quit\n");
      return 0;
}