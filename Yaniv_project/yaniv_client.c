#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE 50 //according to our longest message
#define NAME_SIZE 10
#define TIMER 120

char buffer[BUFFER_SIZE] = {0};
int hand[5]={55,55,55,55,55};                //The cards of the player
int drop_cards[5] = {55,55,55,55,55};        //The cards the player draws
int desk[5]={55,55,55,55,55};           //The cards on the desk
int tcp_sock = -1, mul_sock = -1, hand_size=0, desk_size, num_of_cards_to_drop,yaniv_flag = 0;
ssize_t rec_check,send_check;

void error_hand(int error_num,char* error_msg);
void end_func();
void print_cards(int cards[5], int num);
int comp(const void * a, const void * b);
void print_one_card(int card);
void play();
void drop_cards_func();


int main(int argc,char** argv) {
//variables
    struct timeval timeout;
    struct sockaddr_in serverAddr;
    struct sockaddr_in addr;
    socklen_t addr_size;
    fd_set  set, tmp_set;
    char ip[16] = {0},name[NAME_SIZE+1] = {0},multicast_ip[16] = {0}, s_msg_type[2]={0}, name_len[2]= {0}, P_name[NAME_SIZE+1],shape[8]={0};//arbitrary choice
    int ans;
    int tcp_port, mul_port, tmp_sock ,i ,ip_len , r_msg_type, j, players_num, P_name_len, r_t_now;
    int SN_expose,SN_query,SN_count=0, decision_e, num_to_expose, took_card, card_val, check;
    int flag_start=0, flag_deal=0, error_check, server_sock_closed, YANIV_sum, MY_YANIV_val, win_type;
    struct ip_mreq mreq;
//define the timeout
    timeout.tv_sec = TIMER;
    timeout.tv_usec = 0;
//init, get the variables from the arguments
    strcpy(name,argv[1]);
    strcpy(ip,argv[2]);
    tcp_port = atoi(argv[3]);
    mul_port = atoi(argv[4]);
//open TCP socket and send connect message
    tcp_sock = socket(AF_INET,SOCK_STREAM,0);
    if(tcp_sock == -1){ error_hand(tcp_sock,"socket opening error");}
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ip);//inet_addr
    serverAddr.sin_port = htons(tcp_port);
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
    addr_size = sizeof(serverAddr);
    error_check = connect(tcp_sock,&serverAddr,addr_size);
    if(error_check == -1){ error_hand(error_check,"connect error");}
//arranging the buffer before sending connect message
    memset(buffer,'\0',BUFFER_SIZE);                //clear the buffer
//    printf("please enter your nickname, up to %d letters \n", NAME_SIZE+1);
//    fgets(name,NAME_SIZE+1,stdin);
    buffer[0] = 0;                                                          //enter the msg_type to the buffer
    buffer[1] = strlen(name);                                            //enter the size of the payload (the name in this case)
    strcpy(buffer+2,name);                                        //enter the nick name to the buffer
//send connect message
    send_check = send(tcp_sock,buffer,BUFFER_SIZE,0);
    if(send_check == -1){ error_hand((int)send_check,"send error");}  //just for the check
    memset(buffer,'\0',BUFFER_SIZE);                //clear the buffer
    printf("connect message sent\n");
//for the select
    FD_ZERO(&set);                                           //clear the set
    FD_SET(tcp_sock,&set);                                   //tcp_sock file descriptor to the set
    FD_SET(fileno(stdin),&set);                       //enter interrupt to the set
// select to get the multicast ip by recv the approve msg
    tmp_set = set;
    printf("select, wait for approve message\n");
    error_check = select(FD_SETSIZE,&tmp_set,NULL,NULL,&timeout);
    if(error_check < 0){ error_hand(error_check,"select-approve error");}
    if(error_check == 0){//Timer, the server does not respond
        printf("the server didnt respond with an approve message\n");
        close(tcp_sock);
        exit(EXIT_SUCCESS);
    }
    for(i = 0; i<FD_SETSIZE;i++){      //to check who is set
        if(FD_ISSET(i,&tmp_set)){      //found one that set
            tmp_sock = i;
            if(tmp_sock == tcp_sock){  //did it the tcp sock?
                rec_check = recv(tmp_sock,buffer,BUFFER_SIZE,0);
                if(rec_check==-1) { error_hand((int)rec_check,"receive error");}
                r_msg_type = (int)buffer[0];                                //type
                if(r_msg_type==1){
                    ip_len = (int)buffer[1];                                //len
                    strncpy(multicast_ip,buffer+2,ip_len);      //payload
                }
                else{
                    printf("inappropriate message, logout");
                    end_func();
                }
                //printf("%s",buffer+ip_len+1);
                printf("approve message received\n");
                memset(buffer,'\0',BUFFER_SIZE);                //clear the buffer
            }
            if(tmp_sock == fileno(stdin)){     //did it enter?
                printf("quitting\n");
                end_func();
            }
        }
    }
//open multicast socket
    mul_sock = socket(AF_INET, SOCK_DGRAM,0);
    if(mul_sock == -1){ error_hand(mul_sock,"socket opening error");}
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(mul_port);
    bind(mul_sock,(struct sockaddr *)&addr, sizeof(addr));
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(mul_sock, IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));
// prepare to select
    FD_SET(mul_sock,&set);                     //mul_sock file descriptor to the set
// select, the server received the registration, wait for messages
    flag_start = 0;
    while(1){
        timeout.tv_sec = TIMER;
        timeout.tv_usec = 0;
        tmp_set = set;
        if(flag_start){error_check = select(FD_SETSIZE,&tmp_set,NULL,NULL,&timeout);}
        else{
            printf("select wait for game start message\n");
            error_check = select(FD_SETSIZE,&tmp_set,NULL,NULL,NULL);
        }
        if(error_check < 0){ error_hand(error_check,"select error");}
        if(error_check == 0){//timer
            printf("timeout logging off\n");
            end_func();
        }
        for(i = 0; i<FD_SETSIZE;i++){
            if(FD_ISSET(i,&tmp_set)){
                tmp_sock = i;
                if(tmp_sock == tcp_sock || tmp_sock==mul_sock){
                    rec_check = recv(tmp_sock,buffer,BUFFER_SIZE,0);
                    if(rec_check==-1) { error_hand((int)rec_check,"receive error");}
                    if(rec_check==0) {
                        printf("the server disconnected, logout\n");
                        server_sock_closed = 1;
                        end_func();
                    }
                    r_msg_type = (int)buffer[0];
                    if(flag_start==0 && flag_deal==0){
                        if(r_msg_type == 2) {
                            flag_start = 1;
                            printf("Hi %s, enjoy the game\n", name);
                            printf("the players in this game are:\n");
                            players_num = (int)buffer[1];
                            P_name_len = 0;
                            r_t_now = 2;
                            for (j=0;j<players_num;j++){
                                P_name_len = (int)buffer[r_t_now];
                                r_t_now++;
                                strncpy(P_name,buffer+r_t_now,P_name_len);
                                printf("Player %d: %s\n", j+1, P_name);
                                memset(P_name,'\0',NAME_SIZE);
                                r_t_now += P_name_len;
                            }
                            continue;
                        }
                        else{
                            printf("inappropriate message, logout");
                            end_func();
                        }
                    }
                    if(flag_start==1 && flag_deal == 0){                //Get the hand of the cards
                        if(r_msg_type == 3) {
                            flag_deal = 1;
                            hand_size = 5;
                            for (j = 0; j < 5 ; j++) {
                                hand[j] = (int)buffer[j+1];
                            }

                            printf("Your cards are:\n");
                            qsort(hand,5,sizeof(int),comp);
                            print_cards(hand,5);
                            continue;
                        }
                        else{
                            printf("inappropriate message, logout");
                            end_func();
                        }
                    }
                    if(flag_start==1 && flag_deal==1){
                        switch (r_msg_type) {
                            case 4:                                     //expose cards
                                SN_expose = (int)buffer[1];
                                if(SN_expose != SN_count){
                                    printf("SN not good\n");
                                    end_func();
                                }
                                if(SN_expose == 0){                     //the first expose message
                                    desk[0] = (int)buffer[2];
                                    desk_size = 1;
                                    for (j=1;j<5;j++){
                                        desk[j] = 55;
                                    }
                                    printf("the first card in the desk is: ");
                                    print_one_card(desk[0]);
                                    printf("\n");
                                    SN_count++;
                                    break;
                                }
                                SN_count++;
                                decision_e = (int)buffer[2];
                                num_to_expose = (int)buffer[3];
                                desk_size = num_to_expose;
                                for (j = 0; j < 5 ; j++) {          //save the card that exposed
                                    if(j<num_to_expose){
                                        desk[j] = (int)buffer[j+4];
                                    }
                                    else{desk[j] = 55;}				 //fill the rest of the arr with 55
                                }
                                qsort(desk,5,sizeof(int),comp);     //sort the array
                                switch (decision_e) {
                                    case 1:                                     //the player took card from the deck
                                        strcpy(P_name,buffer+4+num_to_expose);
                                        printf("%s, took card from the deck\n", P_name);
                                        break;
                                    case 2:                                     //the player took card from the visible pile
                                        took_card = (int)buffer[4];
                                        strcpy(P_name,buffer+5+num_to_expose);
                                        //print the card
                                        printf("%s took ", P_name);
                                        print_one_card(took_card);
                                        printf("from the visible pile\n");
                                        break;
                                    default:
                                        break;
                                }
                                printf("The cards that the player drop are:\n");
                                print_cards(desk,num_to_expose);
                                break;
                            case 5:                                     //query
                                SN_query = (int)buffer[1];
                                if(SN_query != SN_expose){
                                    printf("SN not good\n");
                                    end_func();
                                }
                                memset(buffer,'\0',BUFFER_SIZE);                //clear the buffer
                                play();
                                break;
                            case 7:                                     //deal - get card from the deck
                                hand[hand_size-1]=(int)buffer[1];
                                printf("you got this card: ");
                                print_one_card((int)buffer[1]);
                                printf("\n");
                                qsort(drop_cards,5,sizeof (int),comp);
                                printf("your hand is now:\n");
                                print_cards(hand,hand_size);
                                break;
                            case 8:                                     //player left
                                strcpy(P_name,buffer+1);
                                printf("%s left the game\n", P_name);
                                break;
                            case 9:                                     //Yaniv
                                if(yaniv_flag){
                                    memset(buffer,'\0',BUFFER_SIZE);
                                    break;
                                }
                                YANIV_sum = (int)buffer[1];
                                strcpy(P_name,buffer+2);
                                printf("%s has declare YANIV with sum of %d\n", P_name,YANIV_sum);
                                MY_YANIV_val = 0;
                                for(i=0;i<5;i++){                   //check if the player could declare ASSAF
                                    if(hand[i]<54){
                                        MY_YANIV_val = MY_YANIV_val+(hand[i]%13)+1;
                                    }
                                }
                                if(MY_YANIV_val<=YANIV_sum){                              //if the player have better hand from the yaniv, ask him if he want to declare assaf
                                    printf("Do you want to declare ASSAF? if you do enter 1\n");
                                    check = scanf("%d", &ans);
                                    if(check<0){
                                        error_hand(check,"scanf on play func error");
                                    }
                                    if(ans == 1){
                                        memset(buffer,'\0',BUFFER_SIZE);
                                        buffer[0] = 10;
                                        buffer[1] = (char) MY_YANIV_val;
                                        send_check = send(tcp_sock,buffer,BUFFER_SIZE,0);
                                        if(send_check == -1){ error_hand((int)send_check,"send error");}  //just for the check
                                        memset(buffer,'\0',BUFFER_SIZE);
                                    }
                                }
                                break;
                            case 11:                                    //Winner
                                win_type = (int)buffer[1];
                                strcpy(P_name,buffer+2);
                                if(win_type==1){
                                    printf("%s is the winner of this game, he made YANIV\n", P_name);
                                }                       //win with YANIV
                                if(win_type==2){
                                    printf("%s is the winner of this game, he made ASSAF\n", P_name);
                                }                       //win with ASSAF
                                end_func();
                                break;
                            default:
                                printf("inappropriate message, logout\n");
                                end_func();

                        }
                    }
                    memset(buffer,'\0',BUFFER_SIZE);
                }
                if(tmp_sock == fileno(stdin)){
                    printf("quitting\n");
                    end_func();
                }
            }
        }
    }
    return 0;
}

void error_hand(int error_num,char* error_msg){
    errno = error_num;
    perror(error_msg);

    exit(EXIT_FAILURE);
}

void end_func(){
    printf("LOGOUT\n");
    if(tcp_sock != -1){close(tcp_sock);}
    if(mul_sock != -1){close(mul_sock);}
    exit(EXIT_SUCCESS);
}

void print_cards(int cards[5],int num){
    int card_val, i;
    for (i = 0; i < num; i++) {
        card_val = (cards[i])%13+1;
        if(cards[i]==52 || cards[i]==53){
            printf("joker\n");
        }
        switch (card_val) {
            case 1:
                switch (cards[i]) {
                    case 0 ... 12:
                        printf("A heart\n");
                        break;
                    case 13 ... 25:
                        printf("A diamond\n");
                        break;
                    case 26 ... 38:
                        printf("A club\n");
                        break;
                    case 39 ... 51:
                        printf("A spade\n");
                        break;
                }
                break;
            case 2 ... 10:
                switch (cards[i]) {
                    case 0 ... 12:
                        printf("%d heart\n", card_val);
                        break;
                    case 13 ... 25:
                        printf("%d diamond\n", card_val);
                        break;
                    case 26 ... 38:
                        printf("%d club\n", card_val);
                        break;
                    case 39 ... 51:
                        printf("%d spade\n", card_val);
                        break;
                }
                break;
            case 11:
                switch (cards[i]) {
                    case 0 ... 12:
                        printf("J heart\n");
                        break;
                    case 13 ... 25:
                        printf("J diamond\n");
                        break;
                    case 26 ... 38:
                        printf("J club\n");
                        break;
                    case 39 ... 51:
                        printf("J spade\n");
                        break;
                }
                break;
            case 12:
                switch (cards[i]) {
                    case 0 ... 12:
                        printf("Q heart\n");
                        break;
                    case 13 ... 25:
                        printf("Q diamond\n");
                        break;
                    case 26 ... 38:
                        printf("Q club\n");
                        break;
                    case 39 ... 51:
                        printf("Q spade\n");
                        break;
                }
                break;
            case 13:
                switch (cards[i]) {
                    case 0 ... 12:
                        printf("K heart\n");
                        break;
                    case 13 ... 25:
                        printf("K diamond\n");
                        break;
                    case 26 ... 38:
                        printf("K club\n");
                        break;
                    case 39 ... 51:
                        printf("K spade\n");
                        break;
                }
                break;
        }
    }
    return;
}

int comp(const void * a, const void * b)
{
    return ( *(int*)a - *(int*)b );
}

void print_one_card(int card){
    int card_val;
    char shape[8];
    card_val = (card%13)+1;
    //print the cards
    switch (card/13) {
        case 0:
            strcpy(shape,"heart");
            break;
        case 1:
            strcpy(shape,"diamond");
            break;
        case 2:
            strcpy(shape,"club");
            break;
        case 3:
            strcpy(shape,"spade");
            break;
    }
    switch (card_val) {
        case 1:
            printf("A %s ", shape);
            break;
        case 11:
            printf("J %s ", shape);
            break;
        case 12:
            printf("Q %s ", shape);
            break;
        case 13:
            printf("K %s ", shape);
            break;
        case 2 ... 10:
            printf("%d %s ", card_val,shape);
            break;
    }
}

void play() {
    int decision, i, check;
    int card_from_pile_1, card_from_pile_2, chosen_opt_from_pile, YANIV_val=0;
    card_from_pile_1 = desk[0];
    card_from_pile_2 = desk[desk_size-1];
    qsort(hand,5,sizeof (int),comp);
    printf("It's your turn\n");
    printf("Remember, these are your cards:\n");
    print_cards(hand,5);
    printf("The cards on the desk are:\n");
    print_cards(desk,5);
    printf("Pleas choose your move:\n");
    printf("1. Drop cards and pull from the deck\n");
    printf("2. Drop cards and pull from the visible pile\n");
    for(i=0;i<5;i++){                   //check if the player could declare YANIV
        if(hand[i]<54){
            YANIV_val = YANIV_val+(hand[i]%13)+1;
        }
    }
    if(YANIV_val<=7){                   //if the player can declare YANIV, add yhe Declare YANIV option
        printf("3. Declare YANIV!\n");
    }
    printf("Enter your move: ");
    check = scanf("%d", &decision);
    if(check<0){
        error_hand(check,"scanf on play func error");
    }
    for(i=0;i<3;i++){
        if(decision<1 || decision>3 || (decision==3 && YANIV_val>7)){
            if(i==2){
                end_func();
            }
            printf("Invalid input, please select again\n");
            check = scanf("%d", &decision);
            if(check<0){
                error_hand(check,"scanf on play func error");
            }
        }
    }
//till here the player chose his move
    if(decision==3){        //if the player chose to declare YANIV
        buffer[0]=6;
        buffer[1]=3;
        buffer[2]=YANIV_val;
        send_check = send(tcp_sock,buffer,BUFFER_SIZE,0);
        if(send_check == -1){ error_hand(send_check,"send error");}  //just for the check
        memset(buffer,'\0',BUFFER_SIZE);                //clear the buffer
        yaniv_flag = 1;
        return;
    }                       //if the player chose YANIV
    drop_cards_func();
//start build the message type 6 (play) to send
    hand_size = hand_size-num_of_cards_to_drop+1;     //update the size of the hand to be: the player hand less the cards that he drop, include 1 from the deck\visible pile
    qsort(hand,5,sizeof (int),comp);
    memset(buffer,'\0',BUFFER_SIZE);                //clear the buffer
    buffer[0] = 6;
    buffer[1] = decision;
    buffer[2] = num_of_cards_to_drop;
    for(i=0;i<num_of_cards_to_drop;i++){
        buffer[i+3]=drop_cards[i];
    }
    if (decision == 1){
        send_check = send(tcp_sock,buffer,BUFFER_SIZE,0);
        if(send_check == -1){ error_hand((int)send_check,"send error");}  //just for the check
        memset(buffer,'\0',BUFFER_SIZE);                //clear the buffer
        return;
    }
    if (decision==2){
        printf("Thank you, now please choose the card from the desk\n");
        printf("Remember you can choose one of these cards:\n");
//print the card that the player can take
        printf("1. ");
        print_one_card(card_from_pile_1);
        printf("\n");
        if ((desk_size-1)>0){
            printf("2. ");
            print_one_card(card_from_pile_2);
            printf("\n");
        }
        for(i=0;i<3;i++){
            printf("enter your choose\n");
            check = scanf("%d", &chosen_opt_from_pile);
            if(check<0){
                error_hand(check,"scanf on play func error");
            }
            if(chosen_opt_from_pile<1 || chosen_opt_from_pile==2&&card_from_pile_2>=54 || chosen_opt_from_pile>2){  //if the input was invalid
                if(i==2){
                    end_func();
                }
                printf("Invalid input, please select again, you have two more chances\n");
            }
            else{break;}
        }
        if(chosen_opt_from_pile == 1){
            buffer[num_of_cards_to_drop+3] = card_from_pile_1;
            hand[hand_size-1]=card_from_pile_1;
        }
        if(chosen_opt_from_pile == 2){
            buffer[num_of_cards_to_drop+3] = card_from_pile_2;
            hand[hand_size-1]=card_from_pile_2;
        }
        send_check = send(tcp_sock,buffer,BUFFER_SIZE,0);
        if(send_check == -1){ error_hand((int)send_check,"send error");}  //just for the check
        memset(buffer,'\0',BUFFER_SIZE);                //clear the buffer
        qsort(hand,5,sizeof (int),comp);
        printf("your hand is now:\n");
        print_cards(hand,hand_size);
    }
    return;
}

void drop_cards_func(){
    int check, i, j, k, the_same, up_stream, same_shape,hand_indx[5];
    for(i=0;i<2;i++) {
        printf("How many cards do you want to drop?\n");
        check = scanf("%d", &num_of_cards_to_drop);
        if(check<0){
            error_hand(check,"scanf on play func error");
        }
        for (j = 0; j < 3; j++) {                               //two chances to peek the number of cards
            if (num_of_cards_to_drop > hand_size || num_of_cards_to_drop < 1) {
                if(j==2){
                    //call end func
                }
                printf("Invalid input, please select again, you have two more chances\n");
                check = scanf("%d", &num_of_cards_to_drop);
                if (check < 0) {
                    error_hand(check, "scanf on play func error");
                }
            }
        }
        printf("Choose the cards that you want to drop\n");
        printf("select number between 1 to the number of cards in your hand\n");
        for (k = 0; k < num_of_cards_to_drop; k++) {
            printf("enter card number %d: ", k + 1);
            check = scanf("%d", &hand_indx[k]);//the indx is being saved and not the actual value of the card
            drop_cards[k] = hand[hand_indx[k]-1];
            printf("\n");
            if (check < 0) {
                error_hand(check, "scanf on play func error");
            }
        }
        if(num_of_cards_to_drop == 1){
            hand[hand_indx[0]-1] = 55;
            return;
        }
        qsort(drop_cards,5,sizeof (int),comp);
//check the correctness of the input
        the_same = 1;
        for(k=0;(k<num_of_cards_to_drop-1);k++){            //check if it is a case that all the cards are the same
            if((drop_cards[k]%13)!=(drop_cards[k+1]%13)){
                the_same = 0;
            }
        }
        same_shape = 1;
        for(k=0;(k<num_of_cards_to_drop-1);k++){
            if((drop_cards[k]/13)!=(drop_cards[k+1]/13)){
                same_shape = 0;
            }
        }
        up_stream = 0;
        if(same_shape){
            up_stream = 1;
            for(k=0;(k<num_of_cards_to_drop-1);k++){            //check if it is a case that all the cards are the same
                if(drop_cards[k] != (drop_cards[k+1]-1)){
                    up_stream = 0;
                }
            }
        }
        if(the_same==0 && up_stream==0){
            printf("Invalid input, please select again, you have two more chances\n");
        }
        else{
            for(k = 0;k<num_of_cards_to_drop;k++){
                hand[hand_indx[k]-1] = 55;
            }
            qsort(hand,5,sizeof (int),comp);
            return;
        }
    }
}
