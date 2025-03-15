#include <stdio.h>

#define BASE 1000
#define REG_LIM 500

int main(){
    printf("Select: \n1: Static \n2:Dynamic \n");
    int choice;
    int access_addr;
    int physical_addr;
    int logical_addr;
    scanf("%d", &choice);
    printf("Enter the memory location to access: ");
    scanf("%d", &access_addr);
    switch(choice){
        case 1:
            physical_addr=access_addr;
            logical_addr=access_addr-BASE;
            if(logical_addr< (REG_LIM)){
                printf("Valid access. \n");
            }
            else{
                printf("Invalid access. \n");
            }

            break;
        case 2:
            physical_addr=BASE+access_addr;
            logical_addr=access_addr;
            if(logical_addr< (REG_LIM)){
                printf("Valid access. \n");
            }
            else{
                printf("Invalid access. \n");
            }
            break;
        default:
            break;
    }
}
