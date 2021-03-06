
// compile: gcc TrailingTruckClient.c -o TrailingTruck -lmosquitto 


#include <stdio.h>
#include <mosquitto.h>
#include <unistd.h>
#include <time.h>
#include <json-c/json.h>
#include <string.h>
#include <omp.h>

#define BROKER_ADDRESS "0.0.0.0"
#define MQTT_PORT 1883;
#define MQTT_PUB_TOPIC "TrailingTruckTopicPub"
#define MQTT_SUB_TOPIC "HeadTruckTopicPub"

#define NUM_THREADS 2
const int SAFE_DISTANCE= 20;
const int DECOUPLING_DISTANCE= 50;
struct mosquitto * traillingTruck;

typedef enum {
IDLE, //Truck ndoing nothing, maintain the previous state
DECCELERATING,
ACCELERATING,
STOPPING,
TURNING_RIGHT, 
TURNING_LEFT,
EMERGENCY_STOP,
TRUCK_MALFUNCTION,
CONNECTION_LOST,
COUPLING,
DECOUPLING
} TruckState;

//char state_name[11]={"IDLE", "DECCELERATING", "ACCELERATING", "STOPPING", 
struct truck 
{ 
   
   char *TruckID; 
   TruckState state;
   int speed;
   bool obstacle;
   bool connectivity; 
   bool emergency;
   bool malfunction;
   bool carCrossing;	//This value will be set by the cameras
   bool decoupling;	//This value is true when decoupling & false when not 
   int fuel;	//This value is for fuel level (100-80: Full, 70-50: Half-Full, 40-20: low, <20: Very-low) 
   int TPMS;   //This value is for returning pressure of all tyres (which tyre has low pressure)
   bool Temp;	// This value is the tempurature of truck in C (0:Normal, 1:High)
   int IRdistance;
   TruckState Msg_state;
   
}; 

struct truck truck1= {"1", IDLE, 0, 0, 0, 0 ,0 ,0, 0, 100, 1, 0, 20, IDLE};
int test=0;


void messageHandle(struct mosquitto *mosq, const char *msg, time_t end_t)
{
	//Parse whole JSON Message 
	struct json_object *jobj = json_tokener_parse(msg);

	
	//Parse Truck ID
   	struct json_object *ID;
	json_object_object_get_ex(jobj, "TruckID", &ID);
	
	char *T_ID =  json_object_to_json_string(ID);
	//printf("TruckID: %s\n", T_ID);

	if( strstr(T_ID,"1") != NULL || strstr(T_ID,"0") != NULL )	// TruckID represnt number of truck
	{
		
		printf("Thread %d: %s\n",omp_get_thread_num(), json_object_to_json_string(jobj));
		printf("\n");
		//Parse TimeStamp
		struct json_object *ts;
		json_object_object_get_ex(jobj, "TimeStamp", &ts);
		
	 	time_t start_t = json_object_get_int(ts);
	   	double diff_t;

	   	printf("Thread %d: Time Difference: ",omp_get_thread_num());
	   	//printf("%ld\n", start_t);
	   	//printf("%ld\n", end_t);
	   	
	   	diff_t = difftime(end_t, start_t);
	   	printf("%f\n", diff_t);
	   	printf("\n");
	   	
	   	//Check timestamp if time is more than 1, message is discarded 
	   	if(diff_t < 1)
	   	{
		   	//Parse MSG Type
		   	struct json_object *type;
			json_object_object_get_ex(jobj, "MsgType", &type);
			
			char *MsgType =  json_object_to_json_string(type);
			//printf("MSGTYPE: %s\n", MsgType);
			MsgType_to_TruckState(MsgType);
			//printf("TRUCK STATE: %d\n",truck1.state);

		}
	}
	else
	{
		printf("Thread %d: Not intended for Truck1\n", omp_get_thread_num());
	}
	
	free(jobj);
	free(ID);
	
	
}


void MsgType_to_TruckState(char *MsgType)
{
	#pragma omp critical
	{
		if( strstr(MsgType,"STOP") != NULL )
		{
			truck1.Msg_state = STOPPING;
		}
		else if ( strstr(MsgType,"ACCELERATE") != NULL) 
		{
			truck1.Msg_state = ACCELERATING;
		}
		else if ( strstr(MsgType,"DECCELERATE") != NULL )
		{
			truck1.Msg_state = DECCELERATING;
		}
		else if ( strstr(MsgType,"TURN_RIGHT") != NULL )
		{
			truck1.Msg_state = TURNING_RIGHT;
		}
		else if ( strstr(MsgType,"TURN_LEFT") != NULL )
		{
			truck1.Msg_state = TURNING_LEFT;
		}
		else
		{
			truck1.Msg_state=IDLE;
		}
	}
	//printf("MSG STATE IN FUNCTION: %d\n", truck1.Msg_state);

}


void stateMachine_Truck()
{

	switch( truck1.state ){
		case( IDLE ):
		{
			#pragma omp critical
			{
				if( truck1.emergency == true )
				{
					truck1.state = EMERGENCY_STOP;
				}		
				else if( truck1.malfunction == true )
				{	
					truck1.state = TRUCK_MALFUNCTION;
				}
				else if( truck1.connectivity == false )  
				{
					truck1.state = CONNECTION_LOST;
				}
				else if(truck1.carCrossing == true && truck1.decoupling == false)
				{
					truck1.state = DECOUPLING;
					truck1.decoupling = true;
				}
				else if( truck1.carCrossing == false && truck1.decoupling == true )
				{
					truck1.state = COUPLING;
					truck1.decoupling = false;
				}
				else if( truck1.Msg_state == ACCELERATING )
				{
					truck1.state = ACCELERATING;
				}
				else if( truck1.Msg_state == DECCELERATING )
				{
					truck1.state = DECCELERATING;
				}
				else if( truck1.Msg_state == STOPPING )
				{
					truck1.state = STOPPING;
				}
				else if( truck1.Msg_state == TURNING_LEFT )
				{
					truck1.state = TURNING_LEFT;
				}
				else if( truck1.Msg_state == TURNING_RIGHT )
				{
					truck1.state = TURNING_RIGHT;
				}
				
				truck1.Msg_state=IDLE;
			}
				break;
		}
		case( EMERGENCY_STOP ):
		{
			printf("Thread %d: Truck State: EMERGENCY STOP ACTIVATED\n", omp_get_thread_num());
			speed(0, true);
			truck1.state = IDLE;
			break;
		}
		case( CONNECTION_LOST ):
		{ 
			//todo: retry;
			printf("Thread %d: Truck State: Connection Lost\n", omp_get_thread_num());
			break;
		}
		case( DECOUPLING ):
		{
			printf("Thread %d: Truck State: DECOUPLING ACTIVATED\n", omp_get_thread_num());
			decoupling();
			truck1.state = IDLE;
			break;
		}
		case( COUPLING ):
		{
			printf("Thread %d: Truck State: COUPLING ACTIVATED\n", omp_get_thread_num());
			coupling();
			truck1.state = IDLE;
			break;
		}
		case( TRUCK_MALFUNCTION ):
		{
			printf("Thread %d: Truck State: TRUCK MALFUNCTION\n", omp_get_thread_num());
			truck1.state = EMERGENCY_STOP;
			speed(0, true);
			break;
		}
		case( ACCELERATING ):
		{
			printf("Thread %d: Truck State: ACCELERATE\n", omp_get_thread_num());
			speed(truck1.speed+10, false);
			truck1.state = IDLE;
			break;
		}
		case( DECCELERATING ):
		{
			printf("Thread %d: Truck State: DECCELERATE\n", omp_get_thread_num());
			speed(truck1.speed-10, false);
			truck1.state = IDLE;
			break;
		}
		case( STOPPING ):
		{
			printf("Thread %d: Truck State: STOPPING\n", omp_get_thread_num());
			speed(0, false);
			truck1.state = IDLE;
			break;
		}
		case( TURNING_RIGHT ):
		{
			//todo: call Turn Right function;
			printf("Thread %d: Truck State: Turning Right\n", omp_get_thread_num());
			truck1.state = IDLE;
			break;
		}
		case( TURNING_LEFT ):
		{
			//todo: call Turn LEFT function;
			printf("Thread %d: Truck State: Turning LEFT\n", omp_get_thread_num());
			truck1.state = IDLE;
			break;
		}
	
	}
	printf("\n");
}


void speed(int targetSpeed, bool hardStop)
{

	if( hardStop == true )
	{
		printf("EmergencyStop\n");
		truck1.speed = 0;
	}
	else if ( targetSpeed < truck1.speed )
	{
		//printf("Deccelerating\n");
	
		while( targetSpeed < truck1.speed )
		{	
			//todo: safe distance 
			//printf("InsideDeAcc\n");
			truck1.speed -= 10;	
		}
	}
	else
	{
		//printf("Accelerating\n");

		while( targetSpeed > truck1.speed && truck1.IRdistance >= SAFE_DISTANCE )
		{	
			//printf("InsideAcc\n");
			truck1.speed += 10;
		}
	}

}
void Obstacle_Distance_Monitor()
{
	if( truck1.decoupling == false)
	{
		printf("Thread %d: IR Distance: %d\n", omp_get_thread_num(), truck1.IRdistance); 

		if( truck1.IRdistance < SAFE_DISTANCE && truck1.speed >= 0 )
		{
			printf("Obstacle Detected\n");
			truck1.obstacle = true;
			printf("Deccelerating\n");
			truck1.speed -= 10;
		}
		else 
		{
			truck1.obstacle = false;
			speed(truck1.speed + 100, false);
		}
	}
		
}


void decoupling()
{
	while( truck1.IRdistance < DECOUPLING_DISTANCE )
	{
		truck1.speed -=10;
	}
}


void coupling()
{
	speed(truck1.speed + 50, false);
}
void truck_Monitor()
{
	//(100-80: Full, 70-50: Half-Full, 40-20: low, <20: Very-low) 
	char fuelState[20];
	struct json_object *jobj; 
	
	if(truck1.fuel<=100 && truck1.fuel>=80)
	{
		
		strcpy(fuelState,"Full");
	}
	else if(truck1.fuel<=70 && truck1.fuel>=50)
	{
	
		strcpy(fuelState,"Half-Full");
	}
		else if(truck1.fuel<=40 && truck1.fuel>=20)
	{
		//Send Message to Head Truck with fuel state
		strcpy(fuelState,"Low");
	}
		else if(truck1.fuel<=20)
	{
		//Send Message to Head Truck with fuel state
		strcpy(fuelState,"Very-Low");
	}
	
	if( truck1.fuel<=40 && truck1.fuel>=0 || truck1.TPMS !=0 || truck1.Temp !=0)
	{
		jobj = json_object_new_object();
		/* get the current time */
		time_t timer;
		time(&timer);
						
		json_object_object_add(jobj, "TruckID", json_object_new_string(truck1.TruckID));
		json_object_object_add(jobj, "TimeStamp", json_object_new_int64(timer));
		json_object_object_add(jobj, "Fuel", json_object_new_string(fuelState));
		json_object_object_add(jobj, "TPMS", json_object_new_int64(truck1.TPMS));
		json_object_object_add(jobj, "Temperature", json_object_new_int64(truck1.Temp));
		
		
		const char *Msg1 = json_object_to_json_string(jobj);
						
		//printf("Mosquitto Pub: %d\n",mosquitto_publish(traillingTruck, NULL, MQTT_PUB_TOPIC, 1000, Msg1, 1, false));
		mosquitto_publish(traillingTruck, NULL, MQTT_PUB_TOPIC, 1000, Msg1, 1, false);
		free(Msg1);
	}
	//free(jobj);
	
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


void connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    int i;
    if( !result )
	{
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, MQTT_SUB_TOPIC, 1);
        truck1.connectivity = true;
    }
	else
	{
        fprintf(stderr, "Connect failed\n");
        truck1.connectivity = false;
    }
}


void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
    printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);

    for(int i = 1; i < qos_count; i++)
	{
        printf(", %d", granted_qos[i]);
    }
    printf("\n");
}

int main()
{

	int rc = 0;
	
	int mid = 1;
	int ID;
	struct mosquitto * traillingTruck2;
	omp_set_num_threads(3);	//Define number Threads

	mosquitto_lib_init();

	traillingTruck = mosquitto_new("Trailing-Truck", true, NULL);
	
	if(traillingTruck)
	{
	
		//Set callback function, use when necessary
		mosquitto_connect_callback_set(traillingTruck, connect_callback);
		mosquitto_message_callback_set(traillingTruck, message_callback);
		mosquitto_subscribe_callback_set(traillingTruck, subscribe_callback);
		
		rc = mosquitto_connect(traillingTruck, "localhost", 1883, 60);

		while(1)
		{
			// Beginning of parallel region 
			//#pragma omp parallel 
			#pragma omp parallel sections 
			{ 
	
				//ID= omp_get_thread_num();
				
				//if( ID == 0)
				#pragma omp section
				{
					//Waiting for messages
					rc = mosquitto_loop(traillingTruck, -1, 1);
					
					if( rc )
					{

						printf("connection error!\n");

						sleep(1);

						if(mosquitto_reconnect(traillingTruck)!=MOSQ_ERR_SUCCESS)
						{
							truck1.connectivity=false;
						}
					
					}
					//Monitor Fuel consumption and publish to Head Truck
					truck_Monitor();
					test++;
					if(test>10)
					{
					truck1.fuel=70;
					truck1.TPMS=0;
					truck1.Temp=0;
					}

				}
				//else if(ID==1)
				#pragma omp section
				{
					// Monitoring the state of the truck
					stateMachine_Truck();
					//printf("TRUCK STATE: %d\n",truck1.state);
				
				}
				//else if(ID==2)
				#pragma omp section
				{
					// Monitoring the distance
					printf("Thread %d: Monitoring Distance \n",omp_get_thread_num());
					Obstacle_Distance_Monitor();
					truck1.IRdistance = 100;
					printf("\n");
				
				}
				
			}//#pragma omp sections 
		}//While

		mosquitto_disconnect(traillingTruck);
		mosquitto_destroy(traillingTruck);

	}//trailingTruck

	mosquitto_lib_cleanup();
	
	return 0;
}

