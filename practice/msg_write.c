#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#define BUFFER 100
typedef struct msg_buffer{
    long mesg_type;
    char msg_text[BUFFER];
}message;

int main(){
    key_t key;
    int msgid;
    printf("Enter message to be passed: \n");
    read(0, message.msg_text, sizeof(message), 0);
    key=ftok("progfile", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);
    msgrcv=(msgd, &message, sizeof(message),1,0);
    printf("Message recieved: %s /n", message.msg_text);
    msgctl(msgid, IPC_RMID, NULL);
    return 0;
}
