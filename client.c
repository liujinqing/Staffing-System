/*================================================================
 *   Copyright (C) 2021 hqyj Ltd. All rights reserved.
 *   
 *   文件名称：员工管理系统.c
 *   创 建 者：liu
 *   创建日期：2021年06月25日
 *   描    述：客户端1
 *
 ================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#define N 256
#define T 20
#define M 40
#define P 12
#define IP "192.168.0.135"

#define PORT 8887
//操作指令宏定义
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

//可修改项宏定义
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
#define USER "US"	 //普通用户

//查询
#define ALLINFO "AI"	   //查全员信息
#define PRIVATEINFO "PI"   //查个人信息
#define ALLATTEND "AT"	   //查全员考勤
#define PRIVATEATTEND "PT" //查个人考勤

#define ERR_LOG(msg)                                            \
	do                                                        \
	{                                                         \
		perror(msg);                                        \
		printf("%d %s %s\n", __LINE__, __func__, __FILE__); \
		return -1;                                          \
	} while (0)
//name char,age int,NO int, level int, salary int, type char, addr char, phone char
typedef struct
{ //定义通信结构体
	//	char ID;
	char type;
	char type_sub[3];
	char name[T]; //20
	char pswd[T]; //20
	int age;
	int NO;
	int level;
	int salary;
	char depart[T]; //20
	char addr[M];   //40
	char pho[P];    //12
	char text[T];   //20 用来存服务器反馈的信息
} MSG;

int do_regist(int sfd);
int do_login(int sfd, MSG *pusr);
int do_add_stf(int sfd);		//增加人员
int do_del_stf(int sfd);		//删除人员
int do_mod_stf(int sfd, MSG *pusr); //修改人员
int do_search(int sfd, MSG *pusr);	//查询人员信息
int do_attend(int sfd, MSG *pusr);
int do_histroy(int sfd, MSG usr); //历史记录
int do_quit(int sfd);		    //退出
int do_cancel(int sfd);
//信息修改函数
int do_mod_stf_name(int sfd, MSG *usr);
int do_mod_stf_pswd(int sfd, MSG *usr);
int do_mod_stf_age(int sfd, MSG *usr);
int do_mod_stf_num(int sfd, MSG *usr);
int do_mod_stf_level(int sfd, MSG *usr);
int do_mod_stf_salary(int sfd, MSG *usr);
int do_mod_stf_depart(int sfd, MSG *usr);
int do_mod_stf_address(int sfd, MSG *usr);
int do_mod_stf_phone(int sfd, MSG *usr);

//发送修改信息并接收消息反馈信息
int send_recv_mod_info(int sfd, MSG *pusr);
//修改信息输入并判断
void pswd_input_judge(MSG *ppusr);	  //password 输入并判断
void name_input_judge(MSG *ppusr);	  //name 输入并判断
void age_input_judge(MSG *ppusr);	  //age 输入并判断
void NO_input_judge(MSG *ppusr);	  //NO 输入并判断
void level_input_judge(MSG *ppusr);	  //level 输入并判断
void salary_input_judge(MSG *ppusr);  //salary输入并判断
void depart_input_judge(MSG *ppusr);  //depart 输入并判断
void address_input_judge(MSG *ppusr); //address 输入并判断
void phone_input_judge(MSG *ppusr);	  //phone 输入并判断
//打卡签到
int do_sign_in(int sfd, MSG *pusr);
//查询结果接收并显示
int output_sear_res(int sfd, MSG *pusr);
//登陆错误锁30s
void login_err_lock_30s(int *err_count, time_t *t);

int main(int argc, char *argv[])
{
	//1.创建socket
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	printf("sfd = %d\n", sfd);
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

	//3.连接服务器
	if (connect(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		ERR_LOG("connect"); //OK
	MSG usr = {LOGIN, "", "", "", 0, 0, 0, 0, "", "", "", ""};

	while (1)
	{
		memset(&usr, 0, sizeof(usr));
		usr.type = LOGIN;
		system("clear");
		printf("**********************************************\n");
		printf("*********欢迎使用华清远见员工管理系统*********\n");
		printf("*                                            *\n");
		printf("**********************************************\n");
		printf("* 1.注册 2.管理员登陆 3.普通员工登陆 4.退出  *\n");
		printf("**********************************************\n");
		printf("*------------------------------by liujinqing.*\n");

		int choose;
		printf("请选择>>>\n");
		scanf("%d", &choose);
		while (getchar() != 10)
			;
		switch (choose)
		{
		case 1:
			do_regist(sfd); //注册
			break;
		case 2:
			strcpy(usr.type_sub, ADMIN);
			do_login(sfd, &usr); //管理员登陆
			break;
		case 3:
			strcpy(usr.type_sub, USER);
			do_login(sfd, &usr); //普通用户登陆
			break;
		case 4: //退出
			printf("准备退出\n");
			do_quit(sfd);
			goto EXT;
			break;
		default:
			printf("输入错误\n");
		}
		if (choose == 4)
		{
			break; //退出程序
		}
		printf("输入任意字符清屏>>>");
		while (getchar() != 10)
			;
	}
EXT:
	close(sfd);
	return 0;
}

//注册 仅针对普通用户 管理员用户无需注册
int do_regist(int sfd)
{

	MSG usr = {REGIST, "", "", "", 0, 0, 0, 0, "", "", "", ""};
	for (;;)
	{
		system("clear");
		printf("请输入账号:");
		scanf("%s", usr.name);
		while (getchar() != 10)
			;

		printf("请输入密码:");
		scanf("%s", usr.pswd);
		while (getchar() != 10)
			;

		//向服务器发送注册信息
		if (send(sfd, &usr, sizeof(usr), 0) < 0)
			ERR_LOG("send");

		//等待服务器处理  回复"注册成功"
		if (recv(sfd, &usr, sizeof(usr), 0) < 0) //<=
			ERR_LOG("recv");

		printf("usr.text = %s\n", usr.text);

		//	printf("%c,%s,%s,%s\n",usr.type,usr.name,usr.pswd,usr.text);
		if (strcmp(usr.text, "注册成功") == 0)
		{
			printf("注册成功\n");
			break;
		}
		else
		{
			printf("注册失败\n");
			break;
		}
	}
	return 0;
}

//登陆
int do_login(int sfd, MSG *pusr)
{
	int ret = -1;
	int i;
	time_t t = 0, t_c;
	struct tm *tm = NULL;
	int err_count = 0;
	while (1)
	{
		bzero(pusr->name, sizeof(pusr->name));
		bzero(pusr->pswd, sizeof(pusr->pswd));
		printf("please input name (as your account):");
		scanf("%s", pusr->name);
		while (getchar() != 10)
			;
		printf("please input password:");
		scanf("%s", pusr->pswd);
		while (getchar() != 10)
			;

		//发送账号密码给服务器
		if (send(sfd, pusr, sizeof(*pusr), 0) < 0)
			ERR_LOG("send");
		//printf("%c,%s,%s,%s\n", pusr->type, pusr->name, pusr->pswd, pusr->text);
		//等待服务器相应
		ret = recv(sfd, pusr, sizeof(*pusr), 0);
		//printf("ret = %d\n", ret);
		if (ret < 0)
			ERR_LOG("recv");
		//printf("%c,%s,%s,%s\n", pusr->type, pusr->name, pusr->pswd, pusr->text);
		int item;
		MSG source;
		memcpy(&source, pusr, sizeof(source));
		t_c = time(NULL); //登陆时的当前时间
		//printf("source.type = %c source.tpye_sub = %s source.text = %s\n", source.type, source.type_sub, source.text);
		if (strncmp(pusr->text, "admin", 5) == 0 && (t == 0 || (t_c - t) > 30)) //pusr->name 为管理员name
		{												//输入未连续超3次  错误后再次输入等待超>30s
			err_count = 0;
			while (1)
			{
				memcpy(pusr, &source, sizeof(source));
				printf("%c,%s,%s,%s\n", pusr->type, pusr->name, pusr->type_sub, pusr->text);
				system("clear");
				printf(" %s 管理员 登陆成功!\n", pusr->name);
				printf("**********************************************\n");
				printf("*********欢迎使用华清远见员工管理系统*********\n");
				printf("*                                            *\n");
				printf("*   1.增  2.删  3.改  4.查  5.签到 6 .退出* \n");
				printf("**********************************************\n");
				printf("*------------------------------by liujinqing.*\n");
				item = 0;
				printf("请输入要进行的操作选项-->");
				scanf("%d", &item);
				while (getchar() != 10)
					;
				switch (item)
				{
				case 1:
					printf("添加员工\n");
					do_add_stf(sfd); //增加人员
					break;
				case 2:
					printf("删除员工\n");
					do_del_stf(sfd); //删除人员
					break;
				case 3:
					printf("修改员工\n");
					pusr->type = MODIFY;
					do_mod_stf(sfd, pusr); //修改人员 含管理员name信息
					break;
				case 4:
					printf("查询员工\n");
					pusr->type = SEAR;
					do_search(sfd, pusr); //查询人员信息 含用户名 和密码信息
					break;
				case 5:
					printf("打卡签到\n");
					pusr->type = SIGNIN;
					do_sign_in(sfd, pusr); //查询人员信息 含用户名 和密码信息
					break;
				case 6:
					do_quit(sfd); //退出
					goto END;
					break;
				default:
					printf("输入错误\n");
				}
				printf("输入任意字符清屏>>>");
				while (getchar() != 10)
					;
			}
		}
		else if (strncmp(pusr->text, "normal", 6) == 0 && (t == 0 || (t_c - t) > 30)) //pusr->name 为普通用户name
		{
			err_count = 0;
			while (1)
			{
				memcpy(pusr, &source, sizeof(source));
				system("clear");
				printf(" %s 普通用户 登陆成功!\n", pusr->name);
				printf("**********************************************\n");
				printf("*********欢迎使用华清远见员工管理系统*********\n");
				printf("*                                            *\n");
				printf("**********************************************\n");
				printf("*  1.改个人信息 2.查个人信息 3.签到 4.退出   *\n");
				printf("**********************************************\n");
				printf("*------------------------------by liujinqing.*\n");
				item = 0;
				printf("请输入要进行的操作选项-->");
				scanf("%d", &item);
				while (getchar() != 10)
					;
				switch (item)
				{
				case 1:
					printf("修改个人信息\n");
					pusr->type = MODIFY;
					do_mod_stf(sfd, pusr); //修改人员 含name信息
					break;
				case 2:
					printf("查询个人信息\n");
					pusr->type = SEAR;
					do_search(sfd, pusr); //查询人员信息 SEAR 含name 和 text= normal信息
					break;
				case 3: //考勤信息
					printf("打卡签到\n");
					pusr->type = SIGNIN;
					do_sign_in(sfd, pusr); //查询人员信息 含用户名 和密码信息
					break;
				case 4:
					do_quit(sfd); //退出
					goto END;
					break;
				default:
					printf("input error\n");
				}
				printf("输入任意字符清屏>>>");
				while (getchar() != 10)
					;
			}
		}
		else if (strncmp(pusr->text, "ERROR", 5) == 0) //账号密码输入错误请重新输入。
		{
			login_err_lock_30s(&err_count, &t);
		}
		else
			printf("transmit error.\n");
	}
END:
	return 0;
}
void login_err_lock_30s(int *err_count, time_t *t)
{
	(*err_count)++;
	printf("name or password is error %d times,please input again\n", *err_count);
	if (*err_count > 2)
	{
		printf("your input falsely more than 2 times,please wait 30 seconds\n");
		*t = time(NULL);
		//printf("*t = %ld\n", *t);
		int i = 30;
		while (i > 0)
		{
			printf("Countdown %d seconds\r", i);
			fflush(stdout);
			sleep(1);
			i--;
		}
	}
}
int do_add_stf(int sfd) //增加人员
{
	int temp, i, flag;

	printf("add staff\n");
	MSG usr = {ADD, "", "", "", 0, 0, 0, 0, "", "", "", ""};
	printf("请输入要添加员工的信息>>>\n");
	printf("Original name(Maximum 11 characters and cannot be empty):");
	//name 输入并判断
	name_input_judge(&usr);

	/* printf("NO(must be greater than 0):");
	//NO 输入并判断
	NO_input_judge(&usr); */
	printf("age(must be greater than 20 and less than 65):");
	//age 输入并判断
	age_input_judge(&usr);

	printf("level(1 to 5):");
	//level 输入并判断
	level_input_judge(&usr);

	printf("salary(must be greater than 0):");
	//salary输入并判断
	salary_input_judge(&usr);

	printf("department(Maximum 19 characters and cannot be empty):");
	//depart 输入并判断
	depart_input_judge(&usr);

	printf("address(Maximum 63 characters and cannot be empty):");
	//address 输入并判断
	address_input_judge(&usr);

	printf("telphone(Maximum 11 characters and cannot be empty):");
	//phone 输入并判断
	phone_input_judge(&usr);
	//发送修改信息并接收消息反馈信息
	send_recv_mod_info(sfd, &usr);
	return 0;
}
int do_del_stf(int sfd) //删除人员	//还需判断删除是否成功
{
	char temp;
	printf("delete staff\n");
	MSG usr = {DELETE, "", "", "", 0, 0, 0, 0, "", "", "", ""};
	printf("please input name whose will be deleted\n");
	name_input_judge(&usr);
	//printf("%s\n", usr.name);
	printf("please input NO whose will be deleted\n");
	NO_input_judge(&usr);
	temp = '0';
	printf("Do you want to delete the date where name = %s and NO = %d?[Y/N]\n", usr.name, usr.NO);
	scanf("%c", &temp);
	while (getchar() != '\n')
		;
	if (temp == 'Y' || temp == 'y')
	{
		//发送账号密码给服务器
		if (send(sfd, &usr, sizeof(usr), 0) < 0)
			ERR_LOG("send");
		/* printf("%c,%s,%s,%s,%d,%d,%d,%d,%s,%s,%s,%s\n", usr.type, usr.type_sub, usr.name, usr.pswd, usr.age,
			 usr.NO, usr.level, usr.salary, usr.depart, usr.addr, usr.pho, usr.text); */
		//等待服务器相应
		if (recv(sfd, &usr, sizeof(usr), 0) < 0)
			ERR_LOG("recv");

		printf("%s\n", usr.text);
	}
	printf("按任意键+回车 返回\n");
	scanf("%c", &temp);
	while (getchar() != '\n')
		;
	return 0;
}
int do_mod_stf(int sfd, MSG *pusr) //修改人员  含name 信息 和MODIFY
{
	int i, item;
	printf("modify staff\n");
	//printf("%c,%s,%s,%s\n", pusr->type, pusr->name, pusr->type_sub, pusr->text);
	char show_name[20] = "";
	strcpy(show_name, pusr->name);
	MSG source;
	memcpy(&source, pusr, sizeof(source));
	if (strcmp(pusr->text, "admin") == 0)
	{
		while (1)
		{
			memcpy(pusr, &source, sizeof(source));
			system("clear");
			printf(" %s 管理员用户 登陆成功!\n", show_name);
			printf("**********************************************\n");
			printf("*********欢迎使用华清远见员工管理系统*********\n");
			printf("*                                            *\n");
			printf("**********************************************\n");
			printf("*  1.姓名  2.密码  3.年龄  4.工号  5.职级    *\n");
			printf("*  6.薪资  7.部门  8.地址  9.电话  10.退出   *\n");
			printf("**********************************************\n");
			printf("*------------------------------by liujinqing.*\n");
			item = 0;
			printf("请输入要进行的操作选项-->");
			scanf("%d", &item);
			while (getchar() != 10)
				;
			switch (item)
			{
			case 1: //修改姓名
				printf("修改姓名\n");
				strcpy(pusr->type_sub, NAME);
				do_mod_stf_name(sfd, pusr);
				break;
			case 2: //修改密码
				printf("修改密码\n");
				strcpy(pusr->type_sub, PSWD);
				do_mod_stf_pswd(sfd, pusr);
				break;
			case 3: //修改年龄
				printf("修改年龄\n");
				strcpy(pusr->type_sub, AGE);
				do_mod_stf_age(sfd, pusr);
				break;
			case 4: //修改工号
				printf("修改工号\n");
				strcpy(pusr->type_sub, NUM);
				do_mod_stf_num(sfd, pusr);
				break;
			case 5: //修改职级
				printf("修改职级\n");
				strcpy(pusr->type_sub, LEVEL);
				do_mod_stf_level(sfd, pusr);
				break;
			case 6: //修改薪资
				printf("修改薪资\n");
				strcpy(pusr->type_sub, SALARY);
				do_mod_stf_salary(sfd, pusr);
				break;
			case 7: //修改部门
				printf("修改部门\n");
				strcpy(pusr->type_sub, DEPART);
				do_mod_stf_depart(sfd, pusr);
				break;
			case 8: //修改地址
				printf("修改地址\n");
				strcpy(pusr->type_sub, ADDRESS);
				do_mod_stf_address(sfd, pusr);
				do_quit(sfd);
				break;
			case 9: //修改电话
				printf("修改电话\n");
				strcpy(pusr->type_sub, PHONE);
				do_mod_stf_phone(sfd, pusr);
				break;
			case 10:
				do_quit(sfd);
				goto END;
				break;
			default:
				printf("输入错误\n");
			}

			printf("输入任意字符清屏>>>");
			while (getchar() != 10)
				;
		}
	}
	else if (strcmp(pusr->text, "normal") == 0) //修改人员  含name 信息 和MODIFY
	{
		while (1)
		{
			memcpy(pusr, &source, sizeof(source));
			system("clear");
			printf(" %s 普通用户 登陆成功!\n", show_name);
			printf("**********************************************\n");
			printf("*********欢迎使用华清远见员工管理系统*********\n");
			printf("*                                            *\n");
			printf("**********************************************\n");
			printf("*    1.密码    2.年龄    3.部门    4.地址    *\n");
			printf("*    5.电话    6.职级    7.薪资    8.退出    *\n");
			printf("**********************************************\n");
			printf("*------------------------------by liujinqing.*\n");
			item = 0;
			printf("请输入要进行的操作选项-->");
			scanf("%d", &item);
			while (getchar() != 10)
				;
			switch (item)
			{
			case 1:
				printf("修改密码\n");
				strcpy(pusr->type_sub, PSWD);
				do_mod_stf_pswd(sfd, pusr); //MODIFY name normal
				break;
			case 2:
				printf("修改年龄\n");
				strcpy(pusr->type_sub, AGE);
				do_mod_stf_age(sfd, pusr);
				break;
			case 3:
				printf("修改部门\n");
				strcpy(pusr->type_sub, DEPART);
				do_mod_stf_depart(sfd, pusr);
				break;
			case 4:
				printf("修改地址\n");
				strcpy(pusr->type_sub, ADDRESS);
				do_mod_stf_address(sfd, pusr);
				break;
			case 5:
				printf("修改电话\n");
				strcpy(pusr->type_sub, PHONE);
				do_mod_stf_phone(sfd, pusr);
				break;
			case 6:
				printf("修改职级\n");
				strcpy(pusr->type_sub, LEVEL);
				do_mod_stf_level(sfd, pusr);
				break;
			case 7:
				printf("修改薪资\n");
				strcpy(pusr->type_sub, SALARY);
				do_mod_stf_salary(sfd, pusr);
				break;
			case 8:
				do_quit(sfd);
				goto END;
				break;
			default:
				printf("输入错误\n");
			}

			printf("输入任意字符清屏>>>");
			while (getchar() != 10)
				;
		}
	}
END:
	printf("exit do_mod_stf\n");
	return 0;
}
//修改员工姓名
int do_mod_stf_name(int sfd, MSG *pusr) //over
{
	int temp;
	//原始工号NO输入
	printf("Original NO(must be greater than 0):");
	NO_input_judge(pusr); //NO 输入并判断
	printf("New name(Maximum 19 characters and cannot be empty):");
	//name 输入并判断
	name_input_judge(pusr);
	send_recv_mod_info(sfd, pusr);
	return 0;
}
//修改密码
int do_mod_stf_pswd(int sfd, MSG *pusr) //over
{
	printf("do_mod_stf_pswd\n");
	int temp;

	//原始name输入
	printf("Original name(Maximum 19 characters and cannot be empty):");
	name_input_judge(pusr); //name 输入并判断

	printf("New password(Maximum 19 characters and cannot be empty):");
	//password 输入并判断
	pswd_input_judge(pusr);

	//发送修改信息，并接收反馈信息。
	send_recv_mod_info(sfd, pusr);
	return 0;
}
//修改年龄
int do_mod_stf_age(int sfd, MSG *pusr) //over
{
	printf("do_mod_stf_age\n");
	//printf("%c,%s,%s,%s\n", pusr->type, pusr->name, pusr->type_sub, pusr->text);
	int temp;
	if (strcmp(pusr->text, "admin") == 0) //管理员根据NO修改age
	{
		//原始工号NO输入
		printf("Original NO(must be greater than 0):");
		NO_input_judge(pusr); //NO 输入并判断
	}
	printf("New age(must be greater than 20 and less than 65):"); //管理员根据NO修改age
	//age 输入并判断
	age_input_judge(pusr);

	send_recv_mod_info(sfd, pusr);
	return 0;
}
//修改工号
int do_mod_stf_num(int sfd, MSG *pusr) //over
{
	printf("do_mod_stf_num\n");
	//原始name输入
	printf("Original name(Maximum 19 characters and cannot be empty):");
	name_input_judge(pusr); //name 输入并判断
	//phone 输入并判断
	printf("Original telphone(Maximum 11 characters and cannot be empty):");
	phone_input_judge(pusr);

	printf("New NO(must be greater than 0):");
	//NO 输入并判断
	NO_input_judge(pusr);

	//发送修改信息，并接收反馈信息。
	send_recv_mod_info(sfd, pusr);
	return 0;
}
//修改职级
int do_mod_stf_level(int sfd, MSG *pusr) //over
{
	printf("do_mod_stf_level\n");
	if (strcmp(pusr->text, "admin") == 0)
	{
		//原始工号NO输入
		printf("Original NO(must be greater than 0):");
		NO_input_judge(pusr); //NO 输入并判断
	}
	printf("New level(1 to 5):");
	//level 输入并判断
	level_input_judge(pusr);
	//发送修改信息，并接收反馈信息。
	send_recv_mod_info(sfd, pusr);
	return 0;
}
//修改薪资
int do_mod_stf_salary(int sfd, MSG *pusr) //over
{
	printf("do_mod_stf_salary\n");
	if (strcmp(pusr->text, "admin") == 0)
	{
		//原始工号NO输入
		printf("Original NO(must be greater than 0):");
		NO_input_judge(pusr); //NO 输入并判断
	}
	printf("New salary(must be greater than 0):");
	//salary 输入并判断
	salary_input_judge(pusr);
	//发送修改信息，并接收反馈信息。
	send_recv_mod_info(sfd, pusr);
	return 0;
}
//修改部门
int do_mod_stf_depart(int sfd, MSG *pusr) //over
{
	printf("do_mod_stf_depart\n");
	if (strcmp(pusr->text, "admin") == 0)
	{
		//原始工号NO输入
		printf("Original NO(must be greater than 0):");
		NO_input_judge(pusr); //NO 输入并判断
	}
	printf("New department(Maximum 19 characters and cannot be empty):");
	//depart 输入并判断
	depart_input_judge(pusr);

	send_recv_mod_info(sfd, pusr);
	return 0;
}
//地址修改
int do_mod_stf_address(int sfd, MSG *pusr) //over
{
	printf("do_mod_stf_address\n");
	if (strcmp(pusr->text, "admin") == 0)
	{
		//原始工号NO输入
		printf("Original NO(must be greater than 0):");
		NO_input_judge(pusr); //NO 输入并判断
	}
	printf("new address(Maximum 63 characters and cannot be empty):");
	//address 输入并判断
	address_input_judge(pusr);

	send_recv_mod_info(sfd, pusr);
	return 0;
}
//修改phone
int do_mod_stf_phone(int sfd, MSG *pusr) //over
{
	printf("do_mod_stf_phone\n");
	if (strcmp(pusr->text, "admin") == 0)
	{
		//原始工号NO输入
		printf("Original NO(must be greater than 0):");
		NO_input_judge(pusr); //NO 输入并判断
	}
	printf("New telphone(11 characters and cannot be empty):");
	//phone 输入并判断
	phone_input_judge(pusr);

	send_recv_mod_info(sfd, pusr);
	return 0;
}

//发送修改信息并接收消息反馈信息
int send_recv_mod_info(int sfd, MSG *pusr)
{
	int temp;
	//发送修改名字信息给服务器
	if (send(sfd, pusr, sizeof(*pusr), 0) < 0)
		ERR_LOG("send");
	//printf("%c,%s,%s,%s,%d,%d,%d,%d,%s,%s,%s,%s\n", pusr->type, pusr->type_sub, pusr->name, pusr->pswd, pusr->age,
	//	 pusr->NO, pusr->level, pusr->salary, pusr->depart, pusr->addr, pusr->pho, pusr->text);
	//等待服务器相应
	if (recv(sfd, pusr, sizeof(*pusr), 0) < 0)
		ERR_LOG("recv");
	//printf("%s\n", pusr->text);
	printf("按任意键+回车 返回\n");
	scanf("%d", &temp);
	while (getchar() != '\n')
		;
}
//password 输入并判断
void pswd_input_judge(MSG *ppusr) //clear
{
	while (1)
	{
		bzero(ppusr->pswd, sizeof(ppusr->pswd));
		fgets(ppusr->pswd, sizeof(ppusr->pswd), stdin);
		printf("pho =%s\n", ppusr->pswd);
		if (ppusr->pswd[0] == '\n') //是否为空
			printf("password is empty! Please input!\n");
		else if (strlen(ppusr->pswd) < 7)
			printf("the number of input is less than 6!Please input again!\n");
		else if (strlen(ppusr->pswd) < (T - 1) || (strlen(ppusr->pswd) == (T - 1) && ppusr->pswd[T - 2] == '\n')) //是否不足11位
		{
			ppusr->pswd[strlen(ppusr->pswd) - 1] = '\0';
			break;
		}
		else
		{
			while (getchar() != 10) //长度19位，先吸收垃圾字符，然后看是否有非法字符。
				;
			break;
		}
	}
}
//name 输入并判断
void name_input_judge(MSG *ppusr)
{
	//printf("%s\n", usr.name);
	while (1)
	{
		bzero(ppusr->name, sizeof(ppusr->name));
		fgets(ppusr->name, sizeof(ppusr->name), stdin);
		if (ppusr->name[0] == '\n')
			printf("depart is empty! Please input!\n");
		else if (strlen(ppusr->name) < (T - 1) || (strlen(ppusr->name) == (T - 1) && ppusr->name[T - 2] == '\n')) // 需\n位  ---> \0
		{
			ppusr->name[strlen(ppusr->name) - 1] = '\0';
			break;
		}
		else
		{
			while (getchar() != 10) //长度11位，先吸收垃圾字符，然后看是否有非法字符。
				;
			break;
		}
	}
}
//age 输入并判断
void age_input_judge(MSG *ppusr)
{
	while (1)
	{
		scanf("%d", &ppusr->age);
		while (getchar() != 10)
			;
		if (ppusr->age < 20 && ppusr->age > 65)
		{
			printf("input error,please input again\n");
		}
		else
			break;
	}
}
//NO 输入并判断
void NO_input_judge(MSG *ppusr)
{
	while (1)
	{
		scanf("%d", &ppusr->NO);
		while (getchar() != 10)
			;
		if (ppusr->NO <= 0)
		{
			printf("input error,please input again\n");
		}
		else
			break;
	}
}
//level 输入并判断
void level_input_judge(MSG *ppusr)
{
	while (1)
	{
		scanf("%d", &ppusr->level);
		while (getchar() != 10)
			;
		if (ppusr->level < 1 || ppusr->level > 5)
		{
			printf("input error,please input again\n");
		}
		else
			break;
	}
}
//salary输入并判断
void salary_input_judge(MSG *ppusr)
{
	while (1)
	{
		scanf("%d", &ppusr->salary);
		while (getchar() != 10)
			;
		if (ppusr->salary <= 0)
		{
			printf("input error,please input again\n");
		}
		else
			break;
	}
}
//depart 输入并判断
void depart_input_judge(MSG *ppusr)
{
	while (1)
	{
		bzero(ppusr->depart, sizeof(ppusr->depart));
		fgets(ppusr->depart, sizeof(ppusr->depart), stdin);
		if (ppusr->depart[0] == '\n')
			printf("depart is empty! Please input!\n");
		else if (strlen(ppusr->depart) < (T - 1) || (strlen(ppusr->depart) == (T - 1) && ppusr->depart[T - 2] == '\n')) // 需\n位  ---> \0
		{
			ppusr->depart[strlen(ppusr->depart) - 1] = '\0';
			break;
		}
		else
		{
			while (getchar() != 10) //长度11位，先吸收垃圾字符，然后看是否有非法字符。
				;
			break;
		}
	}
}
//address 输入并判断
void address_input_judge(MSG *ppusr)
{
	while (1)
	{
		bzero(ppusr->addr, sizeof(ppusr->addr));
		fgets(ppusr->addr, sizeof(ppusr->addr), stdin);
		if (ppusr->addr[0] == '\n')
			printf("addr is empty! Please input!\n");
		else if (strlen(ppusr->addr) < (M - 1) || (strlen(ppusr->addr) == (M - 1) && ppusr->addr[M - 2] == '\n')) // 需\n位  ---> \0
		{
			ppusr->addr[strlen(ppusr->addr) - 1] = '\0';
			break;
		}
		else
		{
			while (getchar() != 10) //长度11位，先吸收垃圾字符，然后看是否有非法字符。
				;
			break;
		}
	}
}
//phone 输入并判断
void phone_input_judge(MSG *ppusr)
{
	int flag, i;
	while (1)
	{
		flag = 0;
		bzero(ppusr->pho, sizeof(ppusr->pho));
		fgets(ppusr->pho, sizeof(ppusr->pho), stdin);
		printf("pho =%s\n", ppusr->pho);
		if (ppusr->pho[0] == '\n') //是否为空
			printf("telephone is empty! Please input!\n");
		else if (strlen(ppusr->pho) < (P - 1) || (strlen(ppusr->pho) == (P - 1) && ppusr->pho[P - 2] == '\n')) //是否不足11位
			printf("the number of input is less than 11\n");
		else
		{
			while (getchar() != 10) //长度11位，先吸收垃圾字符，然后看是否有非法字符。
				;
			for (i = 0; i < sizeof(ppusr->pho) - 1; i++)
			{
				if (ppusr->pho[i] < 48 || ppusr->pho[i] > 57)
				{
					flag++;
					printf("input invalid char! Please input again!\n");
					break;
				}
			}
			if (flag == 0)
				break;
		}

		//fflush(stdout);
	}
}

int do_search(int sfd, MSG *pusr) //查询是否存在  含用户名和密码信息
{
	int item, ret, temp;
	//printf("%c,%s,%s,%s\n", pusr->type, pusr->name, pusr->type_sub, pusr->text);
	MSG source;
	memcpy(&source, pusr, sizeof(source));
	if (strcmp(pusr->text, "admin") == 0) //管理员根据NO修改age
	{
		while (1)
		{
			memcpy(pusr, &source, sizeof(source));
			system("clear");
			printf(" %s 管理员 登陆成功!\n", pusr->name);
			printf("***********欢迎使用员工管理系统************\n");
			printf("*                                      *\n");
			printf("****************************************\n");
			printf("*      1.个人信息         2.全员信息      *\n");
			printf("*      3.个人考勤         4.全员考勤      *\n");
			printf("*-------------- 5.退出 -----------------*\n");
			item = 0;
			printf("请输入要进行的操作选项-->");
			scanf("%d", &item);
			while (getchar() != 10)
				;
			switch (item)
			{
			case 1:
				printf("查个人信息\n");
				strcpy(pusr->type_sub, PRIVATEINFO);
				printf("请输入要查询员工的姓名>>>\n");
				//原始name输入
				printf("Original name(Maximum 19 characters and cannot be empty):");
				name_input_judge(pusr); //name 输入并判断
				break;
			case 2:
				printf("查全员信息\n");
				strcpy(pusr->type_sub, ALLINFO);
				printf("员工信息表\n");
				break;
			case 3: //考勤信息
				printf("查个人考勤\n");
				strcpy(pusr->type_sub, PRIVATEATTEND);
				printf("请输入要查询员工的姓名>>>\n");
				//原始name输入
				printf("Original name(Maximum 19 characters and cannot be empty):");
				name_input_judge(pusr); //name 输入并判断
				break;
			case 4: //考勤信息
				printf("查全员考勤\n");
				strcpy(pusr->type_sub, ALLATTEND);
				break;
			case 5:
				do_quit(sfd); //退出
				goto NI;
				break;
			default:
				printf("输入错误\n");
			}
			if (item >= 1 && item <= 4)
			{
				output_sear_res(sfd, pusr);

				printf("按任意键+回车 返回\n");
				scanf("%d", &temp);
				printf("输入任意字符清屏>>>");
				while (getchar() != 10)
					;
			}
		}
	}
	else if (strcmp(pusr->text, "normal") == 0) //普通用户 查个人信息
	{
		while (1)
		{
			memcpy(pusr, &source, sizeof(source));
			system("clear");
			printf(" %s 普通用户 登陆成功!\n", pusr->name);
			printf("***********欢迎使用员工管理系统************\n");
			printf("*                                      *\n");
			printf("****************************************\n");
			printf("*    1.个人信息   2.个人考勤   3.退出      *\n");
			printf("*                                       *\n");
			printf("*---------------------------------------*\n");
			item = 0;
			printf("请输入要进行的操作选项-->");
			scanf("%d", &item);
			while (getchar() != 10)
				;
			switch (item)
			{
			case 1:
				printf("查个人信息\n");
				strcpy(pusr->type_sub, PRIVATEINFO);
				break;
			case 2: //考勤信息
				printf("查个人考勤\n");
				strcpy(pusr->type_sub, PRIVATEATTEND);
				break;
			case 3:
				do_quit(sfd); //退出
				goto NI;
				break;
			default:
				printf("输入错误\n");
			}
			if (item == 1 || item == 2)
			{

				output_sear_res(sfd, pusr);
				printf("按任意键+回车 返回\n");
				scanf("%d", &temp);
				printf("输入任意字符清屏>>>");
				while (getchar() != 10)
					;
			}
		}
	}

NI:
	return 0;
}

int output_sear_res(int sfd, MSG *pusr) //
{

	int ret;
	if (strcmp(pusr->type_sub, PRIVATEINFO) == 0 || strcmp(pusr->type_sub, ALLINFO) == 0)
	{
		printf("Name\t\t    Age\t      NO\tLevel\t  Salary    Department\t\tPhone\t\tAddress\n");
		printf("-------------------------------------------------------------------------------------------------------\n");
	}
	else //需修改
	{
		printf("Name\t\t\tAttendance\n");
		printf("---------------------------------------\n");
	}
	//发送查询信息
	if (send(sfd, pusr, sizeof(*pusr), 0) < 0)
		ERR_LOG("send");
	/* printf("%c,%s,%s,%s,%d,%d,%d,%d,%s,%s,%s,%s\n", pusr->type, pusr->type_sub, pusr->name, pusr->pswd, pusr->age,
				 pusr->NO, pusr->level, pusr->salary, pusr->depart, pusr->addr, pusr->pho, pusr->text); */
	while (1)
	{
		//等待服务器相应
		ret = recv(sfd, pusr, sizeof(*pusr), 0);
		if (ret < 0)
			ERR_LOG("recv");
		if (strcmp(pusr->text, "ransmit over") == 0)
		{
			printf("接收完毕\n");
			break;
		}
		if (strcmp(pusr->text, "not found") == 0)
		{
			printf("not found\n");
			break;
		}
		if (strcmp(pusr->type_sub, PRIVATEINFO) == 0 || strcmp(pusr->type_sub, ALLINFO) == 0)
			//printf("%-20s\t%d\t%d\t%d\t%d\t%s\t\t%s\t%s\n", pusr->name, pusr->age, pusr->NO, pusr->level, pusr->salary, pusr->depart, pusr->pho, pusr->addr);
			printf("%-20s%-10d%-10d%-10d%-10d%-20s%-16s%s\n", pusr->name, pusr->age, pusr->NO, pusr->level, pusr->salary, pusr->depart, pusr->pho, pusr->addr);
		else //需修改
		{
			printf("%-20s%-20s\n", pusr->name, pusr->text);
		}
	}
	return 0;
}

int do_sign_in(int sfd, MSG *pusr) //打开签到
{
	//将打卡签到时间导入到考勤表中
	int ret;
	if (send(sfd, pusr, sizeof(*pusr), 0) < 0)
		ERR_LOG("send");
	//等待服务器相应
	ret = recv(sfd, pusr, sizeof(*pusr), 0);
	if (ret < 0)
		ERR_LOG("recv");
	printf("signed in successfully\n");
	printf("name:%s\n", pusr->name);
	printf("time:%s\n", pusr->text);

	return 0;
}
int do_quit(int sfd)
{
	MSG usr = {QUIT, "", "", "", 0, 0, 0, 0, "", "", "", ""};
	if (send(sfd, &usr, sizeof(usr), 0) < 0)
		ERR_LOG("send");
	//printf("%c,%s,%s\n", usr.type, usr.name, usr.pswd);
	return 0;
}
