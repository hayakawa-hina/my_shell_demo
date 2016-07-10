/*61216134*/
/*早川雄登*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

int tty, sigchld_flag = 1;
char path[512] = "";

char *redirect[] = {
	">>",
	"<<",
	"0>",
	"1>",
	"2>",
	"&>",
	"<",
	">",
	"|",
	"&",
	NULL
};

int get_commandline(char *, char *[]);
int get_redirect(int , char *[], char *);
int process_exe(int ac, char *av[]);
int redirect_proc(int , char *[]);
void sig_handler(int sig);

int get_commandline(char *str, char *av[])
{
	int ac = 0;
	while(1){		
		while(*str == ' ' || *str == '\t')
				str++;
		if(*str == '\0')
			break;
		if(*str == '\n'){
			*str == '\0';
			break;
		}
		av[ac] = str;
		while(*str != ' ' && *str != '\n' && *str != '\0' && *str != '\t')
			str++;
		if(*str == '\0')
			break;
		*str++ = '\0';
		ac = get_redirect(ac, av, av[ac]);
	}
	av[ac] = NULL;
	return ac;
}
int get_redirect(int ac, char *av[], char *str)
{
	int i, red = -1;
	int min1 = strlen(str);
	int min2 = min1;
	int max2 = 0;
	char *ret;

	while(1){
		red = -1;
		min1 = strlen(str);
		min2 = min1;
		max2 = 0;
		for(i = 0; redirect[i] != NULL; i++){
			if((ret = strstr(str, redirect[i])) != NULL){
				if((min1 > (int)(ret - str))){
					min1 = (int)(ret - str);				
					max2 = strlen(redirect[i]);
					red = i;	
				}
			}		
		}
		if(red == -1){
			return ++ac;
		}else{
			if(min1 != 0){
				av[ac++] = str;
				str[min1] = '\0';
				av[ac++] = redirect[red];
				if(min2 != (min1 + max2))			
					av[ac] = &str[min1 + max2];
				else
					return ac;
			}else if(min1 == 0 && max2 == strlen(str)){
				av[ac++] = redirect[red];
				return ac;
			}else if(min1 == 0 && max2 != strlen(str)){
				av[ac++] = redirect[red];
				av[ac] = &str[min1 + max2];
			}
			str = av[ac];
		}
	}
}
int pipe_check(int n, int ac, char *av[])
{
	int i, j, k;
	for(i = n + 1; i < ac; i++)
		if(strcmp(av[i], "|") == 0)
			return i;
	return -1;
}

int redirect_error(int ac, char *av[]){

	int i, j, flag = 0;

	for(i = 0; i < ac; i++){
		for(j = 0; redirect[j] != NULL; j++){
			if(strcmp(av[i], redirect[j]) == 0){
				if(flag == 0){
					flag = 1;
					break;
				}else
					return -1;
			}
			flag = (redirect[j + 1] == NULL) ? 0 : flag;
		}
	}
	return 0;
}

int pipe_exe(int ac, char *av[])
{
	int n = -1, i = 0, pn = -1, flag = 0, k = 0, bg_flag = 0;
	pid_t pid, fpgrp;
	char fpgid[32];
	int status[16];
	int pfd[16][2]/*, pgid[2]*/;
	
	
	if(strcmp(av[ac - 1], "&") == 0){
		av[ac - 1] = NULL;
		bg_flag = 1;
		ac--;
	}
	while(1){
		if((n = pipe_check(n , ac, av)) != -1){
			if(flag == 0){
				flag = 1;
				pipe(pfd[i]);
				if((fpgrp = fork()) == 0){
					if(setpgid(0, fpgrp) == -1)
						fprintf(stderr, "setprgp erorr: %d\n", errno);
					av[n] = NULL;
					close(1);
					dup(pfd[i][1]);
					close(pfd[i][0]);
					close(pfd[i][1]);
					redirect_proc(pn + 1, av);
				}
			}else{
				i++;
				pipe(pfd[i]);
				if(fork() == 0){
					if(setpgid(0, fpgrp) == -1)
						fprintf(stderr, "setprgp erorr: %d\n", errno);
					av[n] = NULL;
					close(0);		
					close(1);
					dup(pfd[i-1][0]);
					dup(pfd[i][1]);
					int l;
					for(l = 0; l <= i; l++){
						close(pfd[l][0]);
						close(pfd[l][1]);
					}
					redirect_proc(pn + 1, av);
				}
			}
			pn = n;
		}else{
			if(flag == 0){
				if((fpgrp = fork()) == 0){
					if(setpgid(0, fpgrp) == -1)
						fprintf(stderr, "setprgp erorr: %d\n", errno);
					redirect_proc(pn + 1, av);
				  }
				break;
			}
		
			if(fork() == 0){
				if(setpgid(0, fpgrp) == -1)
						fprintf(stderr, "setprgp erorr: %d\n", errno);
				close(0);
				dup(pfd[i][0]);
				int l;
				for(l = 0; l <= i; l++){
					close(pfd[l][0]);
					close(pfd[l][1]);
				}
				redirect_proc(pn + 1, av);
			}
			i++;
			break;
		}
	}
	int j;
	for(j = 0; j < i; j++){
		close(pfd[j][0]);
		close(pfd[j][1]);
	}
	if(bg_flag == 1){
		tcsetpgrp(tty, getpgrp());
	   	return 0;
	}

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTTOU);
	sigaddset(&mask, SIGTTIN);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	tcsetpgrp(tty, fpgrp);
	//printf("tcgetpgrp = %d\n", tcgetpgrp(tty));
	for(k = 0; k <= i; k++){
		sigchld_flag = 0;
		//signal(SIGCHLD, SIG_IGN);
		wait(&status[k]);
		//signal(SIGCHLD, SIG_DFL);
	}
	sigchld_flag = 1;
	tcsetpgrp(tty, getpgrp());
	//printf("\ntcgetpgrp = %d\n", tcgetpgrp(tty));
	return -1;
}

int redirect_check(int m, char *av[])
{
	int i, j;
	for(i = m + 1; av[i] != NULL; i++)
		for(j = 0; redirect[j] != "|"; j++)
			if(strcmp(av[i], redirect[j]) == 0)
				return i;
	return -1;
}
int redirect_proc(int m, char *av[])
{	
	int i;
   	int fd;
	int pn = m;
	while(1){
		if((m = redirect_check(m, av)) != -1){
			if(strcmp(av[m], ">") == 0){			
				fd = open(av[m+1], O_WRONLY|O_CREAT|O_TRUNC, 0644);		 
				close(1);
				if(dup(fd) == -1){
					printf("dup : error\n");
					exit(1);
				}
				close(fd);
				av[m] = NULL;
			}else if(strcmp(av[m], "<") == 0){
				fd = open(av[m+1], O_RDONLY|O_CREAT, 0644);		 
				close(0);
				if(dup(fd) == -1){
					printf("dup : error\n");
					exit(1);
				}
				close(fd);
				av[m] = NULL;
			}else if(strcmp(av[m], ">>") == 0){			
				fd = open(av[m+1], O_WRONLY|O_CREAT|O_APPEND, 0644);		 
				close(1);
				if(dup(fd) == -1){
					printf("dup : error\n");
					exit(1);
				}
				close(fd);
				av[m] = NULL;
			}else if(strcmp(av[m], "<<") == 0){
				while(1){
					char buf[32];
					char ibuf[128] = "";
					printf(">");
					if(fgets(buf, sizeof buf, stdin) == NULL)
						return 0;
					buf[strlen(buf) - 1] = '\0';
					if(strcmp(buf, av[m+1]) == 0){
						int fd2;
						fd2 = open("<<", O_WRONLY|O_CREAT|O_APPEND, 0644);
						write(fd2, ibuf, sizeof(ibuf));				
						close(fd2);
						break;
					}
					buf[strlen(buf) - 1] = '\n';
					strcat(ibuf, buf);
				}
				fd = open("<<", O_RDONLY, 0644);		 
				close(0);
				if(dup(fd) == -1){
					printf("dup : error\n");
					exit(1);
				}
				close(fd);
				av[m] = NULL;
				}
		}else{
			if(execvp(av[pn], &av[pn]) < 0){
				fprintf(stderr, "command error\n");
				exit(1);
			}
			return 0;
		}
	}
}

void sig_handler(int sig)
{
	if(sig == SIGINT){
		//printf("\ncatch SIGINT\n");
		if(tcgetpgrp(tty) != getpgrp()){
			printf("\n");
			killpg(tcgetpgrp(tty), SIGKILL);
			tcsetpgrp(tty, getpgrp());
		}
		signal(SIGINT, SIG_IGN);
	}else if(sig == SIGCHLD){
	   	if(sigchld_flag == 1){
			int status;
			wait(&status);
		}
		//signal(SIGCHLD, SIG_IGN);
	}
}

void tiruda(int ac, char *av[], char *home)
{
	int i;
	char tmp[256];
	for(i = 0; i < ac; i++){
		if(av[i][0] == '~' && (av[i][1] == '/' || av[i][1] == '\0')){
			strcpy(tmp, home);
			strcat(tmp, &av[1][1]);
			strcpy(av[1], tmp);
		}
	}
}

int main()
{
	int ac = 0;
	char *av[16];
	char buf[1000];
	char tilde[512] = "";	
	char home[128];

	strcpy(home, getenv("HOME"));
	tty = open("/dev/tty", O_RDWR);
	while(1){
		if(signal(SIGINT, sig_handler) == SIG_ERR){
			fprintf(stderr, "signal error\n");
			exit(1);
		}else if(signal(SIGCHLD, sig_handler) == SIG_ERR){
			fprintf(stderr, "signal error\n");
			exit(1);
		}

		getcwd(path, 512);
		printf("[%s]mysh $ ", path);
		
		if(fgets(buf, sizeof buf, stdin) == NULL)
			return 0;
		if(strlen(buf) == 1)			
			continue;
		//	printf("buf : %s\n", buf);
		ac = get_commandline(buf, av);
		tiruda(ac, av, home);
		/*printf("argc : %d\n", ac);
		int i;
		for(i = 0; i < ac; i++)
			printf("av[%d] : (%s)\n", i, av[i]);	   
		*/
		if(redirect_error(ac, av) == -1){
			printf("Redirect or Pipe charcter error\n");
			continue;
		}

		if(strcmp(av[0], "exit") == 0)
			exit(1);
		
		if(strcmp("cd", av[0]) == 0){
			if(ac == 1){
				if(chdir(home) != 0)
					fprintf(stderr, "cd : error\n");
				continue;
			}else{
				if(chdir(av[1]) != 0)
					fprintf(stderr, "cd : error\n");
				continue;
			}
		}
		pipe_exe(ac, av);
	}
	return 0;
}

