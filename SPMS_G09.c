#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

// Define maximum limits
#define MAX_BOOKINGS 1000
#define MAX_NAME_LENGTH 40
#define MAX_PARKING_SLOTS 3
#define MAX_FACILITIES 3 
#define MAX_MEMBER 5
#define BUFFER_SIZE 125
#define facnum 7
#define days 31
#define hour 24

// Structure for forPriority
typedef struct {
    int type;
    int day;
    int hours;
    int duration;
    int facilities[MAX_FACILITIES];
    int facility_count;
    int bookingid;
} forPri;

// Structure for a booking
typedef struct {
    char type[MAX_NAME_LENGTH]; // Type of booking (Parking, Reservation, Event, Essentials)
    char member[MAX_NAME_LENGTH]; // Name of the member
    char date[11]; // Date of booking
    char time[6]; // Time of booking
    float duration; // Duration of booking
    char facilities[MAX_FACILITIES][MAX_NAME_LENGTH]; // Additional facilities (should be at least 2 except essentials)
    int facility_count; // Count of additional facilities
    forPri prio; //data for scheduling
} Booking;


// Array to store bookings
Booking bookings[MAX_BOOKINGS];
int booking_count = 0;
int member_booking_count[MAX_MEMBER] = {0};

int pipes[MAX_MEMBER][2][2];
pid_t pids[MAX_MEMBER];
bool childcreated =0;
int originalbcount=0;

// Array for parking timeslots
//bool parktimeslot[MAX_PARKING_SLOTS][31][24] ={0};
// Array for facilities timeslots
// use MAX_PARKING_SLOTS becuz both all facilities and parking slots are 3
bool timetable[facnum][MAX_PARKING_SLOTS][days][hour] = {0};//fac type, which, day, hours

// ceilling function
int ceilling(float n){
    int temp = n;
    if (n>temp){
        return temp+1;
    }
    return temp;
}

// Function to add a forPri
forPri addforpri(char *type, char *date, char *time, float duration, char facilities[][MAX_NAME_LENGTH], int facility_count, int booking_id){
    forPri newforpri;
    //type
    if (strcmp(type, "Parking")==0){
        newforpri.type = 2;
    }else if (strcmp(type, "Reservation")==0){
        newforpri.type = 1;
    }else if (strcmp(type, "Event")==0){
        newforpri.type = 0;
    }else {//essential
        newforpri.type = 3;
    }
    //day
    char day[3];
    memcpy(day, &date[8], 2);
    day[2] = '\0';
    newforpri.day = atoi(day);
    //time
    char hrs[3];
    memcpy(hrs, &time[0], 2);
    hrs[2] = '\0';
    newforpri.hours = atoi(hrs);
    //duration
    int dura = ceilling(duration);
    newforpri.duration = dura;
    //facilities
    int i;
    for (i=0; i<facility_count; i++){
        //printf("%s; \n", facilities[i]);

        if (strcmp(facilities[i], "locker")==0){
            newforpri.facilities[i] = 1;continue;
        }else if (strcmp(facilities[i], "umbrella")==0){
            newforpri.facilities[i] = 2;continue;
        }else if (strcmp(facilities[i], "battery")==0){
            newforpri.facilities[i] = 3;continue;
        }else if (strcmp(facilities[i], "cable")==0){
            newforpri.facilities[i] = 4;continue;
        }else if (strcmp(facilities[i], "valetpark")==0){
            newforpri.facilities[i] = 5;continue;
        }else if (strcmp(facilities[i], "inflationservice")==0){
            newforpri.facilities[i] = 6;continue;
        }else {
            printf("bugged: %s;\n", facilities[i]);
        }
    }
    newforpri.facility_count = facility_count;
    //bookingid
    newforpri.bookingid = booking_id;  
    return newforpri;
}


// Function to add a booking
void addBooking(char *type, char *member, char *date, char *time, float duration, char facilities[][MAX_NAME_LENGTH], int facility_count) {
    if (booking_count < MAX_BOOKINGS) {
        Booking newBooking;
        // Set the type of booking
        strcpy(newBooking.type, type);
        // Populate the rest of the fields
        strcpy(newBooking.member, member);
        strcpy(newBooking.date, date);
        strcpy(newBooking.time, time);
        newBooking.duration = duration;
        newBooking.facility_count = facility_count;
        int i;
        for (i = 0; i < facility_count; i++) {
            strcpy(newBooking.facilities[i], facilities[i]);
            //printf("lll%slll\n", newBooking.facilities[i]);
        }
        forPri newprio;
        int temp = booking_count;
        newprio = addforpri(type, date, time, duration, facilities, facility_count, temp);
        newBooking.prio = newprio;
        bookings[booking_count++] = newBooking;
        member_booking_count[member[7]-'A']++;
        printf("%s booking added for %s on %s at %s for %.1f hours.\n", type, member, date, time, duration);
    } else {
        printf("Error: Booking limit reached!\n");
    }
}

// Function to add a parking
void addParking(char *member, char *date, char *time, float duration, char facilities[][MAX_NAME_LENGTH], int facility_count) {
    addBooking("Parking", member, date, time, duration, facilities, facility_count);
}

// Function to add a reservation
void addReservation(char *member, char *date, char *time, float duration, char facilities[][MAX_NAME_LENGTH], int facility_count) {
    addBooking("Reservation", member, date, time, duration, facilities, facility_count);
}

// Function to add an event
void addEvent(char *member, char *date, char *time, float duration, char facilities[][MAX_NAME_LENGTH], int facility_count) {
    addBooking("Event", member, date, time, duration, facilities, facility_count);
}

// Function to book essential
void bookEssentials(char *member, char *date, char *time, float duration, char facilities[][MAX_NAME_LENGTH], int facility_count) {
    addBooking("Essentials", member, date, time, duration, facilities, facility_count);
}

// Function to add a batch of bookings from a file
void addBatch(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Could not open file %s.\n", filename);
        return;
    }

    char line[MAX_BOOKINGS];
    while (fgets(line, sizeof(line), file)) {
        char type[MAX_NAME_LENGTH], member[MAX_NAME_LENGTH], date[11], time[6];
        float duration;
        char facilities[MAX_FACILITIES][MAX_NAME_LENGTH];
        int facility_count = 0;

        // Parse the line to extract booking details
        char *token = strtok(line, " ");
        strcpy(type, token);

        token = strtok(NULL, " ");
        strcpy(member, token);

        token = strtok(NULL, " ");
        strcpy(date, token);

        token = strtok(NULL, " ");
        strcpy(time, token);

        token = strtok(NULL, " ");
        duration = atof(token);

        while ((token = strtok(NULL, " \n")) != NULL && facility_count < MAX_FACILITIES) {
            char faclist[6][MAX_NAME_LENGTH] = {"battery", "cable", "locker", "umbrella", "valetpark", "inflationservice"};
            int i, j=0;
            for (i=0;i<6;i++){
                if (strcmp(token , faclist[i])==0){
                    j++;
                }
            }
            if (j==0)continue;
            strcpy(facilities[facility_count++], token);
            //printf("9%s9\n", facilities[facility_count-1]);
        }

        // Call the appropriate function based on the type of booking
        if (strcmp(type, "addParking") == 0) {
            addParking(member, date, time, duration, facilities, facility_count);
        } else if (strcmp(type, "addReservation") == 0) {
            addReservation(member, date, time, duration, facilities, facility_count);
        } else if (strcmp(type, "addEvent") == 0) {
            addEvent(member, date, time, duration, facilities, facility_count);
        } else if (strcmp(type, "bookEssentials") == 0) {
            bookEssentials(member, date, time, duration, facilities, facility_count);
        } else {
            printf("Error: Invalid booking type %s.\n", type);
        }
    }

    fclose(file);
    printf("Batch processing complete for file %s.\n", filename);
}

// empty accept list
void emptyaccept(bool list[MAX_BOOKINGS], int num){
    int i;
    for (i=0;i<num; i++){
        list[i] = 0;
    }
}

// empty timetable
void emptytimetable(bool list[facnum][MAX_PARKING_SLOTS][days][hour]){
   int i,j,k,x;
   for (i=0;i<7;i++){
        for(j=0;j<3;j++){
            for(k=0;k<31;k++){
                for(x=0;x<24;x++)list[i][j][k][x]=0;
            }
        }
   }
}

// time sort prilist
void timesort(forPri list[MAX_BOOKINGS], int num){
    int i, j;
    forPri temp;
    for (i=1; i<num; i++){
        for (j=0; j<num-1; j++){
            if (list[j+1].day<list[j].day){
                temp = list[j];
                list[j] = list[j+1];
                list[j+1] = temp; 
            }else if (list[j+1].day==list[j].day){
                if (list[j+1].hours<list[j].hours){
                    temp = list[j];
                    list[j] = list[j+1];
                    list[j+1] = temp; 
                }
            }
        }
    }
}

//fcfs sort
void fcfssort(Booking list[MAX_BOOKINGS], int num){
    int i, j;
    Booking temp;
    for (i=1; i<num; i++){
        for (j=0; j<num-1; j++){
            if (list[j+1].prio.bookingid<list[j].prio.bookingid){
                temp = list[j];
                list[j] = list[j+1];
                list[j+1] = temp; 
            }
        }
    }
}

//prio sort
void priosort(Booking list[MAX_BOOKINGS], int num){
    int i, j;
    Booking temp;
    //for (i=0;i<num; i++)printf("b4 test bid: %d, type: %s, mem:%s, date%s, time%s, %d\n", list[i].prio.bookingid, list[i].type, list[i].member, list[i].date, list[i].time, list[i].prio.type*1000+list[i].prio.bookingid );

    for (i=1; i<num; i++){
        for (j=0; j<num-1; j++){
            int x,y;
            x = list[j].prio.type*1000+list[j].prio.bookingid;
            y = list[j+1].prio.type*1000+list[j+1].prio.bookingid;
            if (y<x){
                temp = list[j];
                list[j] = list[j+1];
                list[j+1] = temp; 
            }
        }
    }
    //for (i=0;i<num; i++)printf("after test bid: %d, type: %s, mem:%s, date%s, time%s, %d\n", list[i].prio.bookingid, list[i].type, list[i].member, list[i].date, list[i].time, list[i].prio.type*1000+list[i].prio.bookingid );
}

//opti sort 1. small facility count 2. short duration
void optisort(Booking list[MAX_BOOKINGS], int num){
    int i, j;
    Booking temp;
    // for (i=0;i<num; i++)printf("b4 test bid: %d, type: %s, mem: %s, dur: %d, fcnt:%d, %d\n", list[i].prio.bookingid, list[i].type, list[i].member, list[i].prio.duration, list[i].prio.facility_count, list[i].prio.facility_count*1000+list[i].prio.duration*100+list[i].prio.bookingid);

    for (i=1; i<num; i++){
        for (j=0; j<num-1; j++){
            int x,y;
            x = list[j].prio.facility_count*1000+list[j].prio.duration*100+list[j].prio.bookingid;
            y = list[j+1].prio.facility_count*1000+list[j+1].prio.duration*100+list[j+1].prio.bookingid;
            if (y<x){
                temp = list[j];
                list[j] = list[j+1];
                list[j+1] = temp; 
            }
        }
    }
    // for (i=0;i<num; i++)printf("after test bid: %d, type: %s, mem:%s, dur: %d, fcnt%d, %d\n", list[i].prio.bookingid, list[i].type, list[i].member, list[i].prio.duration, list[i].prio.facility_count, list[i].prio.facility_count*1000+list[i].prio.duration*100+list[i].prio.bookingid);
}

// addon for checking availablity    return -1 if not available, otehr will be which facility
int checkavaADDON(bool timetable[facnum][MAX_PARKING_SLOTS][days][hour], int factype, int d, int h, int dura){
    int i,j;
    if (d+1>=days) {
        return -1; //out of bound
    }
    if (dura+h>23){ // hour + duration >23 case
        int leftover = dura+h-24; //eg 23 + 3 -24 = 2 | duration: 23-02 total 3 hours
        for (j=0; j<MAX_PARKING_SLOTS; j++){
            bool k =1;
            bool flag=0;
            for (i=0; i<leftover; i++){
                if (timetable[factype][j][d+1][i]==1){
                    k= 0;
                    flag = true;
                    break;
                }
            }
            if (flag)continue;
            for (i=h; i<24;i++){
                if (timetable[factype][j][d][i]==1){
                    k= 0;
                    break;
                }
            }
            if (k){
                return j;
            }
        }
    }else{
        for (j=0; j<MAX_PARKING_SLOTS; j++){
            bool k=1;
            for (i=h; i<h+dura;i++){ //eg 14 + 3 = 17 | duration 14-17 total 3hours
                if (timetable[factype][j][d][i]==1){
                    k= 0;
                    break;
                }
            }
            if (k){
                return j;
            }
        }
    }
    return -1;
}

//check availability  return 0 if not available, otehr will be which facility
bool checkavailability(forPri prio, bool timetable[facnum][MAX_PARKING_SLOTS][days][hour]){
    if(prio.type!=3){ //type 3 is essentials, so this check if parking slot is available
        if(checkavaADDON(timetable, 0, prio.day, prio.hours, prio.duration)==-1){
            //printf("bid: %d\n", prio.bookingid);
            return 0;
        }
    }
    int i;
    for (i=0; i<prio.facility_count; i++){
        if(checkavaADDON(timetable, prio.facilities[i], prio.day, prio.hours, prio.duration)==-1){
            //printf("bid: %d\n", prio.bookingid);
            return 0;
        }
    }
    return 1;
}

//update timetable
void updatetimetable(forPri prio, bool timetable[facnum][MAX_PARKING_SLOTS][days][hour]){
    int dura= prio.duration, h=prio.hours, d=prio.day, i,j;
    int which; 
    if(prio.type!=3){ //type 3 is essentials, so this check if parking slot is available
        which = checkavaADDON(timetable, 0, d, h, dura);
        if (dura+h>23){ // hour + duration >23 case
            int leftover = dura+h-24; //eg 23 + 3 -24 = 2 | duration: 23-02 total 3 hours
            
            for (i=0; i<leftover; i++){
                timetable[0][which][d+1][i] =1;
            }
            for (i=h; i<24;i++){
                timetable[0][which][d][i]=1;
            }
        }else{
            for (i=h; i<h+dura;i++){ //eg 14 + 3 = 17 | duration 14-17 total 3hours
                timetable[0][which][d][i]=1;
            }
        }
    }
    for (j=0; j<prio.facility_count; j++){
        //printf("j: %d\n", j);
        which = checkavaADDON(timetable, prio.facilities[j], d, h, dura);

        if (dura+h>23){ // hour + duration >23 case
            int leftover = dura+h-24; //eg 23 + 3 -24 = 2 | duration: 23-02 total 3 hours
            for (i=0; i<leftover; i++){
                timetable[prio.facilities[j]][which][d+1][i] =1;
            }
            for (i=h; i<24;i++){
                timetable[prio.facilities[j]][which][d][i]=1;
            }
        }else{
            for (i=h; i<h+dura;i++){ //eg 14 + 3 = 17 | duration 14-17 total 3hours
                timetable[prio.facilities[j]][which][d][i]=1;
            }
        }
    }
}

//utilization of timetable for facilities:  (xx%)
float utilfac(bool timetable[facnum][MAX_PARKING_SLOTS][days][hour], int type){
    int i, j, k;
    float sum=0, total=0;
    for (i=0; i<MAX_PARKING_SLOTS;i++){
        for (j=0; j<31;j++){
            for (k=0;k<24;k++){
                if (timetable[type][i][j][k]==1){
                    sum++;
                }
                total++;
            }
        }
    }
    //printf("sum: %d, total: %d\n", sum, total);

    return sum*100/total;
}

// accept list usage (sum)
int acptusage(bool list[MAX_BOOKINGS], int count){
    int i, sum=0;
    for (i=0; i<count;i++){
        if(list[i]==1){
            sum++;
        }
    }
    //printf("sum: %d\n", sum);
    return sum;
}

// Schedulers 
// FCFS (only for writing in member accept list, not display)
void fcfs(int pipes[MAX_MEMBER][2][2], bool timetable[facnum][MAX_PARKING_SLOTS][days][hour], Booking bookings[], int book_num){
    int i;
    char receive[BUFFER_SIZE];
    char send[BUFFER_SIZE];
    fcfssort(bookings, book_num);
    emptytimetable(timetable);
    for (i = 0; i<MAX_MEMBER; i++){ //tell child to do fcfs function
        write(pipes[i][0][1], "fcfs", BUFFER_SIZE);
    }

    for (i = 0; i<book_num; i++){
        if(checkavailability(bookings[i].prio, timetable)==1){ //check if availible
            updatetimetable(bookings[i].prio, timetable);
            char temp = bookings[i].member[7];
            sprintf(send, "%d", i);
            //printf("%d\n", i); //c which bid is accepted
            write(pipes[temp-'A'][0][1], send, BUFFER_SIZE); //send booking id that is ok
            read(pipes[temp-'A'][1][0], receive, BUFFER_SIZE);
            if(strcmp(receive, "fcfsok")==0)continue; //wait child finish
        }
    }

    for (i = 0; i<MAX_MEMBER; i++){ //tell all child is done
        write(pipes[i][0][1], "fcfsdone", BUFFER_SIZE);
    }
}

// PRIO (only for writing in member accept list, not display)
void prios(int pipes[MAX_MEMBER][2][2], bool timetable[facnum][MAX_PARKING_SLOTS][days][hour], Booking bookings[], int book_num){
    int i;
    char receive[BUFFER_SIZE];
    char send[BUFFER_SIZE];
    priosort(bookings, book_num);
    emptytimetable(timetable);
    for (i = 0; i<MAX_MEMBER; i++){ //tell child to do prio function
        write(pipes[i][0][1], "prio", BUFFER_SIZE);
    }

    for (i = 0; i<book_num; i++){
        //printf(bookings[i].member);
        if(checkavailability(bookings[i].prio, timetable)==1){ //check if availible
            updatetimetable(bookings[i].prio, timetable);
            char temp = bookings[i].member[7];
            sprintf(send, "%d", bookings[i].prio.bookingid);
            // printf("accept bid: %d, for child: %c\n", bookings[i].prio.bookingid, temp); //c which bid is accepted
            write(pipes[temp-'A'][0][1], send, BUFFER_SIZE); //send booking id that is ok
            read(pipes[temp-'A'][1][0], receive, BUFFER_SIZE);
            if(strcmp(receive, "priook")==0)continue; //wait child finish
        }
    }

    for (i = 0; i<MAX_MEMBER; i++){ //tell all child is done
        write(pipes[i][0][1], "priodone", BUFFER_SIZE);
    }
}

//opti
void opti(int pipes[MAX_MEMBER][2][2], bool timetable[facnum][MAX_PARKING_SLOTS][days][hour], Booking bookings[], int book_num){
    int i;
    char receive[BUFFER_SIZE];
    char send[BUFFER_SIZE];
    optisort(bookings, book_num);
    emptytimetable(timetable);
    for (i = 0; i<MAX_MEMBER; i++){ //tell child to do opti function
        write(pipes[i][0][1], "opti", BUFFER_SIZE);
    }

    for (i = 0; i<book_num; i++){
        //printf(bookings[i].member);
        if(checkavailability(bookings[i].prio, timetable)==1){ //check if availible
            updatetimetable(bookings[i].prio, timetable);
            char temp = bookings[i].member[7];
            sprintf(send, "%d", bookings[i].prio.bookingid);
            // printf("accept bid: %d, for child: %c\n", bookings[i].prio.bookingid, temp); //c which bid is accepted
            write(pipes[temp-'A'][0][1], send, BUFFER_SIZE); //send booking id that is ok
            read(pipes[temp-'A'][1][0], receive, BUFFER_SIZE);
            if(strcmp(receive, "optiok")==0)continue; //wait child finish
        }
    }

    for (i = 0; i<MAX_MEMBER; i++){ //tell all child is done
        write(pipes[i][0][1], "optidone", BUFFER_SIZE);
    }
}

// all addon
void alladdon(int pipes[MAX_MEMBER][2][2], bool timtable[facnum][MAX_PARKING_SLOTS][days][hour], int booking_count, FILE *file){
    int i;
    int sum=0.0;
    char receive[BUFFER_SIZE]; 
    for (i=0; i<MAX_MEMBER; i++){
        write(pipes[i][0][1], "all", BUFFER_SIZE);
        read(pipes[i][1][0], receive, BUFFER_SIZE);
        //printf("receive: %s\n", receive);
        sum += atoi(receive);
        //printf("%f\n",sum);
    }
    printf("\tTotal Number of Bookings Received: %d\n", booking_count);
    printf("\t      Number of Bookings Assigned: %d (~%.2f%%)\n", sum, (float)sum*100/(float)booking_count);
    printf("\t      Number of Bookings Rejected: %d (~%.2f%%)\n\n", booking_count-sum, ((float)booking_count-sum)*100/(float)booking_count);
    printf("\tUtilization of Time Slot:\n");
    printf("\tlockers\t\t\t- %.2f%%\n", utilfac(timetable, 1));
    printf("\tumbrellas\t\t- %.2f%%\n", utilfac(timetable, 2));
    printf("\tbatteries\t\t- %.2f%%\n", utilfac(timetable, 3));
    printf("\tcables\t\t\t- %.2f%%\n", utilfac(timetable, 4));
    printf("\tvalet parkings\t\t- %.2f%%\n", utilfac(timetable, 5));
    printf("\tinflation services\t- %.2f%%\n\n", utilfac(timetable, 6));
    // write to file
    fprintf(file, "\tTotal Number of Bookings Received: %d\n", booking_count);
    fprintf(file, "\t      Number of Bookings Assigned: %d (~%.2f%%)\n", sum, (float)sum * 100 / (float)booking_count);
    fprintf(file, "\t      Number of Bookings Rejected: %d (~%.2f%%)\n\n", booking_count - sum, ((float)booking_count - sum) * 100 / (float)booking_count);
    fprintf(file, "\tUtilization of Time Slot:\n");
    fprintf(file, "\tlockers\t\t\t- %.2f%%\n", utilfac(timetable, 1));
    fprintf(file, "\tumbrellas\t\t- %.2f%%\n", utilfac(timetable, 2));
    fprintf(file, "\tbatteries\t\t\t- %.2f%%\n", utilfac(timetable, 3));
    fprintf(file, "\tcables\t\t\t- %.2f%%\n", utilfac(timetable, 4));
    fprintf(file, "\tvalet parkings\t\t- %.2f%%\n", utilfac(timetable, 5));
    fprintf(file, "\tinflation services\t- %.2f%%\n\n", utilfac(timetable, 6));
}

//all 
void all(int pipes[MAX_MEMBER][2][2], bool timetable[facnum][MAX_PARKING_SLOTS][days][hour], Booking bookings[MAX_BOOKINGS], int book_num, FILE *file){
    printf("\n*** Parking Booking Manager - Summary Report ***\n\n");
    printf("Performance:\n\n");
    fprintf(file, "\n*** Parking Booking Manager - Summary Report ***\n\n");
    fprintf(file, "Performance:\n\n");

    fcfs(pipes, timetable, bookings, book_num);
    printf(" For FCFS:\n");
    fprintf(file, " For FCFS:\n");
    sleep(2);

    alladdon(pipes, timetable, book_num, file);

    prios(pipes, timetable, bookings, book_num);
    printf(" For PRIO:\n");
    fprintf(file, " For PRIO:\n");
    sleep(2);
    alladdon(pipes, timetable, book_num, file);

    opti(pipes, timetable, bookings, book_num);
    sleep(2);
    printf(" For OPTI:\n");
    fprintf(file, " For OPTI:\n");
    alladdon(pipes, timetable, book_num, file);


}

// Display accept
void displayaccept(int pipes[MAX_MEMBER][2][2], Booking bookings[MAX_BOOKINGS]){
    int i, j;
    bool check = true;
    char receive[BUFFER_SIZE]="", endtime[6]="";
    fcfssort(bookings, booking_count);
    for (i = 0; i<MAX_MEMBER; i++){ //tell child to do fcfs function
        write(pipes[i][0][1], "acceptdisplay", BUFFER_SIZE);
        printf("\nMember_%c has the following bookings:\n\n", 'A'+i);
        printf("Date\t\tStart\tEnd\tType\t\tDevice\n");
        printf("===================================================================\n");
        check=true;
        while(check){
            write(pipes[i][0][1], "acceptsend", BUFFER_SIZE);
            read(pipes[i][1][0], receive, BUFFER_SIZE);
            if(strcmp(receive, "acceptdone")==0){
                check =false;
                break;
            }
            //printf(receive); //c which bid is being print
            //printf("\n");
            
            int bid = atoi(receive);

            strcpy(endtime, bookings[bid].time); //imagine endtime is 25:00 = =
            int newhr = bookings[bid].prio.duration+bookings[bid].prio.hours;
            if(newhr<10){
                endtime[0] = '0';
                endtime[1] = '0'+newhr;
            }else{
                endtime[0] = '0'+newhr/10;
                endtime[1] = '0'+newhr%10;
            }
            printf("%s\t%s\t%s\t%s\t", bookings[bid].date, bookings[bid].time, endtime, bookings[bid].type);
            if(strcmp(bookings[bid].type, "Parking")==0||strcmp(bookings[bid].type, "Event")==0)printf("\t");
            for (j=0;j<bookings[bid].facility_count; j++){
                printf("%s\t", bookings[bid].facilities[j]);
            }
            printf("\n");
        }
    }
    printf("\t\t-End-\n");
    printf("===================================================================\n\n");
}

//Display reject list
void displayreject(int pipes[MAX_MEMBER][2][2], Booking bookings[MAX_BOOKINGS]){
    int i, j;
    bool check = true;
    char receive[BUFFER_SIZE]="", endtime[6]="";
    for (i = 0; i<MAX_MEMBER; i++){ //tell child to do fcfs function
        write(pipes[i][0][1], "rejectdisplay", BUFFER_SIZE);
        printf("\nMember_%c has the following bookings:\n\n", 'A'+i);
        printf("Date\t\tStart\tEnd\tType\t\tDevice\n");
        printf("===================================================================\n");
        check=true;
        while(check){
            write(pipes[i][0][1], "rejectsend", BUFFER_SIZE);
            read(pipes[i][1][0], receive, BUFFER_SIZE);
            if(strcmp(receive, "rejectdone")==0){
                check =false;
                break;
            }
            //printf(receive); //c which bid is being print
            //printf("\n");
            
            int bid = atoi(receive);

            strcpy(endtime, bookings[bid].time); //imagine endtime is 25:00 = =
            int newhr = bookings[bid].prio.duration+bookings[bid].prio.hours;
            if(newhr<10){
                endtime[0] = '0';
                endtime[1] = '0'+newhr;
            }else{
                endtime[0] = '0'+newhr/10;
                endtime[1] = '0'+newhr%10;
            }
            printf("%s\t%s\t%s\t%s\t", bookings[bid].date, bookings[bid].time, endtime, bookings[bid].type);
            if(strcmp(bookings[bid].type, "Parking")==0||strcmp(bookings[bid].type, "Event")==0)printf("\t");
            for (j=0;j<bookings[bid].facility_count; j++){
                printf("%s\t", bookings[bid].facilities[j]);
            }
            printf("\n");
        }
    }
    printf("\t\t-End-\n");
    printf("===================================================================\n\n");
}

// Child process function
void child_process(int child_id, int pipe_in[2], int pipe_out[2], forPri prilist[MAX_BOOKINGS], int book_num) {
    close(pipe_in[1]);  // Close unused write end of input pipe
    close(pipe_out[0]); // Close unused read end of output pipe
    char receive[BUFFER_SIZE]="";
    char send[BUFFER_SIZE]=""; 
    int finished = 0;
    bool acceptlist[MAX_BOOKINGS] ={0};
    //printf("cid %d\n", child_id);

    while (!finished) {
        read(pipe_in[0], receive, BUFFER_SIZE); // Read message
        int i; 
        //printf("%s %d\n", receive, child_id); //check which message was received

        // fcfs
        if(strcmp(receive, "fcfs")==0){ //fcfs scheduler for accept list
            emptyaccept(acceptlist, book_num);
            bool sdone = true;
            while(sdone){
                read(pipe_in[0], receive, BUFFER_SIZE); // Read message
                if(strcmp(receive, "fcfsdone")==0){
                    sdone = false;
                    //printf("child %d fcfs accept list done\n", child_id);
                    continue;
                }
                for (i=0;i<book_num; i++){ //replace bid to true
                    if(prilist[i].bookingid==atoi(receive)){
                        acceptlist[i] = 1;
                        break;
                    }
                }
                write(pipe_out[1], "fcfsok", BUFFER_SIZE); //tell parent is done
            }
        }
        // prio
        else if(strcmp(receive, "prio")==0){ //prio scheduler for accept list
            emptyaccept(acceptlist, book_num);
            bool sdone = true;
            while(sdone){
                read(pipe_in[0], receive, BUFFER_SIZE); // Read message
                // printf(receive);
                // printf(" child %d\n", child_id);
                if(strcmp(receive, "priodone")==0){
                    sdone = false;
                    //printf("child %d prio accept list done\n", child_id);
                    continue;
                }
                for (i=0;i<book_num; i++){ //replace bid to true
                    if(prilist[i].bookingid==atoi(receive)){
                        //printf("child %d, bid: %d, i:%d\n", child_id, prilist[i].bookingid, i);
                        acceptlist[i] = 1;
                        break;
                    }
                }
                write(pipe_out[1], "priook", BUFFER_SIZE); //tell parent is done
            }
        } 
        
        // opti
        else if (strcmp(receive, "opti")==0){ //opti scheduler for accept list
            emptyaccept(acceptlist, book_num);
            bool sdone = true;
            while(sdone){
                read(pipe_in[0], receive, BUFFER_SIZE); // Read message
                if(strcmp(receive, "optidone")==0){
                    sdone = false;
                    //printf("child %d opti accept list done\n", child_id);
                    continue;
                }
                for (i=0;i<book_num; i++){ //replace bid to true
                    if(prilist[i].bookingid == atoi(receive)){
                        acceptlist[i] = 1;
                        break;
                    }
                }
                write(pipe_out[1], "optiok", BUFFER_SIZE); //tell parent is done
            }
        }

        // all
        else if (strcmp(receive, "all")==0){
            int out = acptusage(acceptlist, book_num);   
            sprintf(send, "%d", out);
            //printf("%s\n", send);
            write(pipe_out[1], send, BUFFER_SIZE);
        }

        // accept display
        else if(strcmp(receive, "acceptdisplay")==0){
            bool check =1;
            for (i=0; i<book_num; i++){
                if (!acceptlist[i]){
                    continue;
                }
                read(pipe_in[0], receive, BUFFER_SIZE); // wait parent say send
                sprintf(send, "%d", prilist[i].bookingid);
                write(pipe_out[1], send, BUFFER_SIZE);
                check=1;
            }
            read(pipe_in[0], receive, BUFFER_SIZE); // wait parent say done
            write(pipe_out[1], "acceptdone", BUFFER_SIZE);
        }

        // reject display
        else if(strcmp(receive, "rejectdisplay")==0){
            bool check =1;
            for (i=0; i<book_num; i++){
                if (acceptlist[i]){
                    continue;
                }
                read(pipe_in[0], receive, BUFFER_SIZE); // wait parent say send

                sprintf(send, "%d", prilist[i].bookingid);
                write(pipe_out[1], send, BUFFER_SIZE);
                check=1;
            }
            read(pipe_in[0], receive, BUFFER_SIZE); // wait parent say done
            write(pipe_out[1], "rejectdone", BUFFER_SIZE);
        }


        // end child
        else if (strcmp(receive, "end")==0)finished=1;
    }
    //printf("bug\n");
    close(pipe_in[0]);
    close(pipe_out[1]);
    exit(0);
}

// create child 
void createchild(){
    if (childcreated&&originalbcount!=booking_count){
        int i;
        for (i=0; i<MAX_MEMBER; i++){
            write(pipes[i][0][1], "end", BUFFER_SIZE);
            waitpid(pids[i], NULL, 0);
            close(pipes[i][0][0]);
            close(pipes[i][0][1]);
            close(pipes[i][1][0]);
            close(pipes[i][1][1]);
        }
    }
    // distribute priority list
    forPri prilist[MAX_MEMBER][MAX_BOOKINGS]; // priority lists for members
    int i,j,k;
    for (j = 0; j<MAX_MEMBER; j++){
        k=0;
        //printf("%d\n",j);
        for (i = 0; i < MAX_BOOKINGS; i++){ // each member grant a priority list
            char temp = bookings[i].member[7];
            if (temp=='A'+j){ //  for saving important info to make schedulering 
                //printf("%dbid\n", bookings[i].prio.bookingid);
                prilist[j][k++] = bookings[i].prio;
            }
        }
        if (k != member_booking_count[j]){
            printf("%d priority distributing had bug, k+1: %d, suppose: %d\n", j, k, member_booking_count[j]);
        }
    }

    // Set up pipes and fork child processes
    for (i = 0; i < MAX_MEMBER; i++) {
        pipe(pipes[i][0]); // Pipe for parent to child
        pipe(pipes[i][1]); // Pipe for child to parent
        pids[i] = fork();
        if (pids[i] == 0) {
            // In child process
            //printf("%d\n", member_booking_count[i]);
            timesort(prilist[i], member_booking_count[i]);
            child_process(i, pipes[i][0], pipes[i][1], prilist[i], member_booking_count[i]);
        } 
        else if (pids[i] < 0) {
            perror("fork");
            return;
        }
    }
    childcreated=true;
    originalbcount = booking_count;
}

void printBookings(const char *algorithm) {
    if (booking_count == 0) {
        printf("No bookings to display.\n");
        return;
    }

    if (strcmp(algorithm, "fcfs") == 0) {
        createchild();
        fcfs(pipes, timetable, bookings, booking_count);
        sleep(1);
        printf("*** Parking Booking - ACCEPT / FCFS ***\n");
        displayaccept(pipes, bookings);
        sleep(1);
        printf("*** Parking Booking - REJECTED / FCFS ***\n");
        displayreject(pipes, bookings);
    }
    else if (strcmp(algorithm, "prio") == 0) {
        createchild();
        prios(pipes, timetable, bookings, booking_count);
        sleep(1);
        printf("*** Parking Booking - ACCEPT / PRIO ***\n");
        displayaccept(pipes, bookings);
        sleep(1);
        printf("*** Parking Booking - REJECTED / PRIO ***\n");
        displayreject(pipes, bookings);
    }
    else if (strcmp(algorithm, "opti") == 0) {
        createchild();
        opti(pipes, timetable, bookings, booking_count);
        sleep(1);
        printf("*** Parking Booking - ACCEPT / OPTI ***\n");
        displayaccept(pipes, bookings);
        sleep(1);
        printf("*** Parking Booking - REJECTED / OPTI ***\n");
        displayreject(pipes, bookings);
    }
    else if (strcmp(algorithm, "ALL") == 0) {
        createchild();
        FILE *file = fopen("SPMS_Report_G09.txt", "w");
        all(pipes, timetable, bookings, booking_count, file);
        fclose(file);
        printf("Report saved to SPMS_Report_G09.txt\n");
    }
    else {
        printf("Error: Invalid algorithm '%s'. Example algorithm: 'fcfs', 'prio', 'opti', 'ALL'.\n", algorithm);
    }
}

bool facerrorhandle(char facilities[3][MAX_NAME_LENGTH], int num){
    char faclist[6][MAX_NAME_LENGTH] = {"battery", "cable", "locker", "umbrella", "valetpark", "inflationservice"};
    int i, j, count=0;
    for(i= 0; i<num; i++){
        for (j=0;j<6;j++){
            if (strcmp(facilities[i], faclist[j])==0){
                count++;
                continue;
            }
        }
    }
    //printf("num: %d, count: %d\n", num, count);
    return count==num;
}

void Input(char *input) {
    char command[20], member[20], date[20], time[20], facility[MAX_NAME_LENGTH];
    float duration;
    char facilities[3][MAX_NAME_LENGTH]; // Assume max 3 facilities
    int facility_count;

    sscanf(input, "%s", command);

    if (strcmp(command, "addParking") == 0) {
        int parsed = sscanf(input, "%*s -%s %s %s %f %s %s %s", member, date, time, &duration, facilities[0], facilities[1], facilities[2]);
        facility_count = parsed - 4;
        if(!facerrorhandle(facilities, facility_count)){
            printf("Error: Invalid facility provided. Valid facilities are: battery, cable, locker, umbrella, valetpark, inflationservice.\n");
            return;
        }
        addParking(member, date, time, duration, facilities, facility_count);
        printf("-> [Pending]\n");
    } else if (strcmp(command, "addReservation") == 0) {
        int parsed = sscanf(input, "%*s -%s %s %s %f %s %s %s", member, date, time, &duration, facilities[0], facilities[1], facilities[2]);
        facility_count = parsed - 4;
        if(!facerrorhandle(facilities, facility_count)){
            printf("Error: Invalid facility provided. Valid facilities are: battery, cable, locker, umbrella, valetpark, inflationservice.\n");
            return;
        }
        addReservation(member, date, time, duration, facilities, facility_count);
        printf("-> [Pending]\n");
    } else if (strcmp(command, "addEvent") == 0) {
        int parsed = sscanf(input, "%*s -%s %s %s %f %s %s %s", member, date, time, &duration, facilities[0], facilities[1], facilities[2]);
        facility_count = parsed - 4;
        if(!facerrorhandle(facilities, facility_count)){
            printf("Error: Invalid facility provided. Valid facilities are: battery, cable, locker, umbrella, valetpark, inflationservice.\n");
            return;
        }
        addEvent(member, date, time, duration, facilities, facility_count);
        printf("-> [Pending]\n");
    } else if (strcmp(command, "bookEssentials") == 0) {
        int parsed = sscanf(input, "%*s -%s %s %s %f %s", member, date, time, &duration, facilities[0]);
        facility_count = 1;
        if(!facerrorhandle(facilities, facility_count)){
            printf("Error: Invalid facility provided. Valid facilities are: battery, cable, locker, umbrella, valetpark, inflationservice.\n");
            return;
        }
        bookEssentials(member, date, time, duration, facilities, facility_count);
        printf("-> [Pending]\n");
    }else if (strcmp(command, "addBatch") == 0) {
        char filename[50];
        sscanf(input, "%*s -%s", filename);
        addBatch(filename);
        printf("-> [Pending]\n");
    } else if (strcmp(command, "printBookings") == 0) {
        char algorithm[10];
        sscanf(input, "%*s -%s", algorithm);
        printBookings(algorithm);
        printf("-> [Done!]\n");
    } else if (strcmp(command, "endProgram") == 0) {
        printf("-> Bye!\n");
        // Wait for child processes to finish
        int i;
        for (i = 0; i < MAX_MEMBER; i++) {
            write(pipes[i][0][1], "end", BUFFER_SIZE);
            waitpid(pids[i], NULL, 0);
            close(pipes[i][0][0]);
            close(pipes[i][0][1]);
            close(pipes[i][1][0]);
            close(pipes[i][1][1]);
        }
        exit(0); // end the program completely
    } else {
        printf("Invalid command: %s\n", command);
        printf("Please try again\n");
    }
}


int main() {
        char input[100];
    printf("~~ WELCOME TO PolyU ~~\n");
    while(1) {
        printf("Please enter booking:\n");
        fgets(input, 100, stdin);
        
        input[strcspn(input, ";")] = '\0'; // remove ";"
        input[strcspn(input, "\n")] = '\0'; // remove newline
        // input "addParking ...;\n" --> "addParking ..."
    
        Input(input);
     }
 
//---------------------------------------------------------------------------------------------------------------------------------------------------------------

    return 0;
}