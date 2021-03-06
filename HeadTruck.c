
// compile: gcc HeadingTruckClient.c -o HeadingTruck -lmosquitto 

#include <stdio.h>
#include <stdlib.h>
#include <mosquitto.h>
#include <unistd.h>
#include <time.h>
#include <json-c/json.h>

#define BROKER_ADDRESS "0.0.0.0"
#define MQTT_PORT 1883;
#define MQTT_PUB_TOPIC "HeadTruckTopicPub"
#define MQTT_SUB_TOPIC "TrailingTruckTopicPub"

enum MsgType{
STOP, 
ACCELERATE, 
DECCELERATE,
TURN_RIGHT,
TURN_LEFT,
NO_ACTION
};

const char *TruckID[6] = { "0", "1", "2", "3", "4", "5" };		//"0" for all trucks
const char *Type[6]    = { "STOP", "ACCELERATE", "DECCELERATE", "TURN_RIGHT", "TURN_LEFT", "NO_ACTION" };
const char *Message[7] = { "Truck Added to Platoon", "GPS Coordinates: 25.339494, 55.364813  ", "Turn Direction: Right", "Turn Direction: Left", "Accelerate to Target Speed 80","Deccelerate to Target Speed 50", "End of Trip" };
int test=0;

struct json_object *Mobj; 
const char *Msg1;


void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	int i;
    if(!result)
	{
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, MQTT_SUB_TOPIC, 1);
        printf("connect callback, rc=%d\n", result);
    }
	else
	{
        fprintf(stderr, "Connect failed\n");
    }
	
}


void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
    int i;
    printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
    for(i=1; i<qos_count; i++)
	{
        printf(", %d", granted_qos[i]);
    }
    printf("\n");
}


void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    
 /* get the current time */
	time_t end_t;
	time(&end_t);
        
	const char *msgS = message->payload; 
	//printf("%s",msgS);

    if(message->payloadlen)
	{
        //printf("%s %s", message->topic, msg);
        messageHandle(mosq, msgS, end_t);
    }
	else
	{
        printf("%s (null)\n", message->topic);
    }

    fflush(stdout);
}
void messageHandle(struct mosquitto *mosq, const char *msg, time_t end_t)
{
	//Parse whole JSON Message 
	struct json_object *jobj = json_tokener_parse(msg);
	printf("%s\n", json_object_to_json_string(jobj));
	printf("\n");
	
	//Parse Truck ID
   	struct json_object *ID;
	json_object_object_get_ex(jobj, "TruckID", &ID);
	
	char *T_ID =  json_object_to_json_string(ID);
	printf("TruckID: %s\n", T_ID);
	printf("\n");

	
		//Parse TimeStamp
		struct json_object *ts;
		json_object_object_get_ex(jobj, "TimeStamp", &ts);
		
	 	time_t start_t = json_object_get_int(ts);
	   	double diff_t;

	   	printf("Time Difference: ");
	   	//printf("%ld\n", start_t);
	   	//printf("%ld\n", end_t);
	   	
	   	diff_t = difftime(end_t, start_t);
	   	printf("%f\n", diff_t);
	   	printf("\n");
	   	
	   	//Check timestamp if time is more than 1, message is discarded 
	   	if(diff_t < 1)
	   	{
		   	//Parse Fuel State
		   	struct json_object *fuel, *TPMS, *Temp;
			json_object_object_get_ex(jobj, "Fuel", &fuel);
			
			char *fuelCons =  json_object_to_json_string(fuel);
			printf("Fuel Consumption: %s\n", fuelCons);
			
			//Parse TPMS State 
			json_object_object_get_ex(jobj, "TPMS", &TPMS);
			
			char *TPMS_State =  json_object_to_json_string(TPMS);
			printf("TPMS State: %s\n", TPMS_State);
			
			//Parse Temp State 
			json_object_object_get_ex(jobj, "Temperature", &Temp);
			
			char *Temp_State =  json_object_to_json_string(Temp);
			printf("Temperature: %s\n", Temp_State);
			printf("\n");
		}
	
	
	
	free(jobj);
	free(ID);
	
}
void ConstructMessage(int tID,enum MsgType t, int M)
{
	/* get the current time */
	time_t timer;
	time(&timer);
	
	Mobj = json_object_new_object();
					
	json_object_object_add(Mobj, "TruckID", json_object_new_string(TruckID[tID]));
	json_object_object_add(Mobj, "TimeStamp", json_object_new_int64(timer));
	json_object_object_add(Mobj, "MsgType", json_object_new_string(Type[t]));
	json_object_object_add(Mobj, "Msg", json_object_new_string(Message[M]));
	
	Msg1 = json_object_to_json_string(Mobj);
	free(Mobj);
	
}

int main()
{
	
	int rc;
	struct mosquitto * headTruck;
	mosquitto_lib_init();
	headTruck = mosquitto_new("Head-Truck", true, NULL);
	
	if( headTruck )
	{

		mosquitto_connect_callback_set(headTruck, connect_callback);
		mosquitto_message_callback_set(headTruck, message_callback);
    		mosquitto_subscribe_callback_set(headTruck, subscribe_callback);

		rc = mosquitto_connect(headTruck, "localhost", 1883, 60);

		

		while (1)
		{
					
			test++;
			if(test==1 && test<20)
			{	
				//Add first trailing trucks to platoon
				ConstructMessage(1, NO_ACTION, 0);
				mosquitto_publish(headTruck, NULL, MQTT_PUB_TOPIC, 1000, Msg1, 1, false);
				free(Msg1);	
			}
			
	
			if(test==4 && test<20)
			{	
				//Add second trailing trucks to platoon
				ConstructMessage(2, NO_ACTION, 0);
				mosquitto_publish(headTruck, NULL, MQTT_PUB_TOPIC, 1000, Msg1, 1, false);
				free(Msg1);
			}
					
			
			if(test==8 && test<20)
			{	
				//Send Destination GPS Coordinates to all Trucks
				ConstructMessage(0, NO_ACTION, 1);
				mosquitto_publish(headTruck, NULL, MQTT_PUB_TOPIC, 1000, Msg1, 1, false);
				free(Msg1);
			}
			
			if(test== 12 && test<20)
			{	
				//Send Accelerate command to all Trucks
				ConstructMessage(0, ACCELERATE, 4);
				mosquitto_publish(headTruck, NULL, MQTT_PUB_TOPIC, 1000, Msg1, 1, false);
				free(Msg1);
			}
			
			if(test== 16 && test<20)
			{	
				//Send Turn Right command to all Trucks
				ConstructMessage(0, TURN_RIGHT, 2);
				mosquitto_publish(headTruck, NULL, MQTT_PUB_TOPIC, 1000, Msg1, 1, false);
				free(Msg1);
				test=0;
			}	
			rc = mosquitto_loop(headTruck, -1, 1);

			if(rc)
			{
				printf("connection error!\n");
				sleep(1);
				mosquitto_reconnect(headTruck);
			}// Connection error

			//sleep(1);
			
		}// While loop

		mosquitto_disconnect(headTruck);
		mosquitto_destroy(headTruck);

	}

	mosquitto_lib_cleanup();

	return 0;
}

