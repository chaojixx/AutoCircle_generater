#include <string.h>
#include <pthread.h>
#include <ros/ros.h>
#include <ros/transport_hints.h>
#include <tf/transform_datatypes.h>
//#include "bullet/bullet3-2.83.7/src/LinearMath/btVector3.h"
//#include "bullet/bullet3-2.83.7/src/LinearMath/btTransform.h"
//#include "bullet/bullet3-2.83.7/src/LinearMath/btQuaternion.h"
#include "raven_2/raven_automove.h"
#include "raven_2/raven_state.h"
#include "DS1.h" //struct param_pass is defined here.

#define ROS_PUBLISH_RATE 10

using namespace raven_2;
using namespace std;

//---------------------------global variables---------------------------
ros:: Subscriber sub_automove;
ros:: Publisher pub_ravenstate;

raven_2::raven_state CURR_RAVEN_STATE;

int SUB_COUNT;
bool SHOW_PUB;
static struct param_pass data1;
tf::Quaternion Q_ori[2];

pthread_t rt_thread;

//-------------------------function declaration-------------------------
void autoincrCallback(raven_2::raven_automove);
int init_ravenstate_publishing(ros::NodeHandle &);
int init_ros(int, char**);
void init_sys();

void displayRcvdMsg();

void publish_raven_state_ros();
void *rt_process(void*);

//---------------------------------main---------------------------------
int main(int argc, char **argv)
{
	init_sys();
	if(init_ros(argc,argv))
	{
		cerr<<"ERROR! Fail to init ROS. Exiting.\n";
		exit(1);
	}
	
	pthread_create(&rt_thread,NULL,rt_process,NULL);

	ros::spin();
	return 0;
}

//--------------------------function definition--------------------------
/**
*	\fn void *rt_process(void *)
*
* 	\brief this thread is dedicated to ros publishing of raven_state data
*
* 	\param a pointer to void
*
*	\return void
*/
void *rt_process(void*)
{
	if(ros::isInitialized())
	{
		while(ros::ok())
		{
			publish_raven_state_ros();
		}
		cout<<"rt_process is shutdown."<<endl;
		return( NULL);
	}
	else
		return 0;
}



/**
*	\fn void autoincrCallback(raven_2::raven_automove msg)
*
*	\brief This function is automatically called whenever someone publish to the raven_automove topic
*
* 	\param raven_2::raven_automove msg
*
*	\return void
*/
void autoincrCallback(raven_2::raven_automove msg) //this was in local_io.cpp
{
	tf::Transform in_incr[2];

	tf::transformMsgToTF(msg.tf_incr[0], in_incr[0]);
	tf::transformMsgToTF(msg.tf_incr[1], in_incr[1]);

	for (int i=0;i<2;i++)
	{
		//add position increment
		tf::Vector3 tmpvec = in_incr[i].getOrigin();
      		data1.xd[i].x += int(tmpvec[0]);
      		data1.xd[i].y += int(tmpvec[1]);
		data1.xd[i].z += int(tmpvec[2]);

		//add rotation increment
		tf::Quaternion q_temp(in_incr[i].getRotation());
		if (q_temp != tf::Quaternion::getIdentity())
		{
			int armidx = 1; //just for simplicity for now

			Q_ori[armidx] = q_temp*Q_ori[armidx];
			tf::Matrix3x3 rot_mx_temp(Q_ori[armidx]);
			for (int j=0;j<3;j++)
			for (int k=0;k<3;k++)
				data1.rd[i].R[j][k] = rot_mx_temp[j][k];
			
		}
	}
	
	//display updated data1
	displayRcvdMsg();


	//publish raven_state
	//..
}


/**
*	\fn int init_ravenstate_publishing(ros::NodeHandle &n) 
*
*	\brief initialize the pub & sub relation.
*
* 	\param int argc, char** argv
*
*	\return int
*/
int init_ravenstate_publishing(ros::NodeHandle &n)  //this was in local_io.cpp
{
	//not include this part for now:
	//..

	pub_ravenstate = n.advertise<raven_state>("raven_state",1);
	sub_automove = n.subscribe<raven_automove>("raven_automove",1,autoincrCallback,ros::TransportHints().unreliable());

}


/**
*	\fn int init_sys() 
*
*	\brief initialize ROS and connect the pub & sub relation.
*
* 	\param void
*
*	\return void
*/
void init_sys()
{
	SHOW_PUB = false;
	Q_ori[0] =  tf::Quaternion::getIdentity();
	Q_ori[1] =  tf::Quaternion::getIdentity();
	cout<<"Welcome to the Auto Circle Generator for RAVEN2"<<endl;
	cout<<"I'm waiting for the talker..."<<endl;
	 
}


/**
*	\fn int init_ros(int argc, char** argv) 
*
*	\brief initialize ROS and connect the pub & sub relation.
*
* 	\param int argc, char** argv
*
*	\return int
*/
int init_ros(int argc, char** argv)  //this was in rt_process_preempt.cpp
{
	//initialize counter
	SUB_COUNT = 0;

	//initialize ros 
	ros::init(argc,argv,"r2_control",ros::init_options::NoSigintHandler);
	
	//establish ros pub & sub relation
	static ros::NodeHandle n;
	init_ravenstate_publishing(n);
	
	return 0;
}


/**
*	\fn int displayRcvdMsg()
*
*	\brief Display the recieved data from the Auto Circle Generator.
*
* 	\param void
*
*	\return void
*/
void displayRcvdMsg()
{
	ROS_INFO("listenerAutoCircle subscribe: raven_automove[%d]", SUB_COUNT);
	SUB_COUNT ++;

	for(int i=0; i<2; i++) 
	{
		//display position increment
		cout<<"\tdata1.xd["<<i<<"] = ";
		cout<<"("<<data1.xd[i].x<<","<<data1.xd[i].y<<","<<data1.xd[i].z<<")"<<endl;

		//display rotation increment
		cout<<"\tdata1.rd["<<i<<"] = ";
		for (int j=0;j<3;j++)
		{	
			cout<<endl<<"\t\t";
			for (int k=0;k<3;k++)
			{
				cout<<data1.rd[i].R[j][k]<<"\t";
			}
		}
		cout<<endl;
	}

	SHOW_PUB = true;
}


/**
*	\fn void publish_raven_state_ros()
*
*	\brief publish the auto circle command to raven_automove topic on ROS.
*
* 	\param void
*
*	\return void
*/
void publish_raven_state_ros()
{
	static int PUB_COUNT = 0;
	static ros::Rate loop_rate(ROS_PUBLISH_RATE);
	static raven_state msg_raven_state;

	//prepare the data to publish
	//..
	for(int i=0;i<6;i++)
	{
		msg_raven_state.pos[i] = PUB_COUNT;
		msg_raven_state.pos_d[i] = PUB_COUNT;
	}
	
	pub_ravenstate.publish(msg_raven_state);
	
	ros::spinOnce();
	
	if(SHOW_PUB)
	{
		cout<<endl<<endl;
		ROS_INFO("listenerAutoCircle publish: raven_state[%d]", PUB_COUNT);
		SHOW_PUB = false;
	}

	loop_rate.sleep();
	PUB_COUNT ++;
}
