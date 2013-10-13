#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc,char *argv[])
{
    int sockfd,new_fd;
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    int sin_size,i;

    //����TCP�׽ӿ�
    //AF_INET: Internet IP Protocol
    //SOCK_STREAM: Sequenced, reliable, connection-based byte streams
    //0: IPPROTO_IP = 0, Dummy protocol for TCP
    if((sockfd = socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        printf("create socket error");
        perror("socket");
        exit(1);
    }
    
    ////��ʼ��sockaddr_in�ṹ�壨��ַ��ͨ����������2323�˿�
    my_addr.sin_family = AF_INET;
    //host byte order to net
    my_addr.sin_port = htons(2323);
    //INADDR_ANY: Address to accept any incoming messages
    my_addr.sin_addr.s_addr = INADDR_ANY;
    //#define sin_zero __pad
    bzero(&(my_addr.sin_zero),8);
    
    ////���׽ӿ�
    if(bind(sockfd,(struct sockaddr *)&my_addr,sizeof(struct sockaddr))==-1)
    {
        perror("bind socket error");
        exit(1);
    }
    
    ////���������׽ӿ�
    //N connection requests will be queued before further requests are refused.
    if(listen(sockfd,10)==-1)
    {
        perror("listen");
        exit(1);
    }
    
    ////�ȴ�����
    while(1)
    {
        sin_size = sizeof(struct sockaddr); //either sockaddr or sockaddr_in can work normally
        
        ////����������ӣ�������һ��ȫ�µ��׽���
        if((new_fd = accept(sockfd,(struct sockaddr *)&their_addr,&sin_size))==-1)
        {
            perror("accept");
            exit(1);
        }
        
        ////����һ���ӽ�������ɺͿͻ��˵ĻỰ�������̼�������
        //fork: Return -1 for errors, 0 to the new process
        if(!fork())
        {
            ////��ȡ�ͻ��˷�������Ϣ
            int numbytes;
            char buff[512];
            memset(buff,0,512);
            
            if((numbytes = recv(new_fd,buff,sizeof(buff),0))==-1)
            {
                perror("recv");
                exit(1);
            }
            for(i=0;i<512;i++)
                printf("%x ",buff[i]);
            puts("");
           
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
    
    close(sockfd);
}