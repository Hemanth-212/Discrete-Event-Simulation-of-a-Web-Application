#include<queue>
using namespace std;

/*
    Event class represents all the events that change the state of the system.
    The five types of events are:
    1. Arrival
    2. Departure
    3. Context switch
    4. Time slice expire
    5. Time out
*/

class Event{
    public:
    int request_id;
    int thread_id;
    int core_id;
    int event_type;
    double event_time; // Time the event has occured
    double request_time; // Time the request to which this event belongs has occured
    double time_out_time; // Time the request times out
    double total_service_left; // Total service time left for the request

    Event(int r_id,int t_id,int c_id,int e_type,double e_time,double s_time,double t_time, double ts_left){
        request_id = r_id;
        thread_id = t_id;
        core_id = c_id;
        event_type = e_type;
        event_time = e_time;
        request_time = s_time;
        time_out_time = t_time;
        total_service_left = ts_left;
    }
};

// Custom compare used for priority queue
class Compare{
    public:
    bool operator()(Event &e1, Event &e2){
        return e1.event_time > e2.event_time;
    }
};

// Event list has all the event that occurs and returns the next event that occurs.
class Eventlist{
    public:
    priority_queue<Event,vector<Event>,Compare>event_list;
    void addEvent(Event e){
        event_list.push(e);
    }
    Event getNextEvent(){
        Event e = event_list.top();
        event_list.pop();
        return e;
    }

    Event peekNextEvent(){
        return event_list.top();
    }

};



