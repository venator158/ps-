#include <windows.h>
#include <stdio.h>

DWORD WINAPI ThreadFunction(LPVOID, IpParam){
    printf("Thread working ... \n");
    Sleep(2000);
    printf("thread finished \n");
    return 0;
}

int main(){
    HANDLE hThread;
    DWORD threadId;

    hThread=CreateThread(
        NULL,
        0,
        ThreadFunction,
        NULL,
        0,
        &threadId
    );

    if(hThread==NULL){
        printf("Failed to create thread, Error: %lu \n", GetLastError());
        return 1;
    }


}