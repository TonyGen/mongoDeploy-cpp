/* */

#ifndef MONGO_DEPLOY_H_
#define MONGO_DEPLOY_H_

#include <utility>
#include <vector>
#include <mongo/client/dbclient.h>
#include <cluster/cluster.h>

namespace mongoDeploy {

/** Prefix for data directory, a number get appended to this, eg. "/data/db" + "1" */
extern std::string mongoDbPathPrefix;
/** Default MongoD/S config is merged with user supplied config. User config options take precedence */
extern program::Options defaultMongoD;
extern program::Options defaultMongoS;

/** mongod program spec with given options +
 * unique values generated for dbpath and port options if not already supplied +.
 * defaultMongoD where not already supplied. */
program::Program mongoD (program::Options opts = program::emptyOptions);

/** mongos program spec with given options +
 * unique values generated for dbpath and port options if not already supplied +.
 * defaultMongoS where not already supplied. */
program::Program mongoS (program::Options opts = program::emptyOptions);

/** Host and port of a mongoD/S process */
std::string hostPort (remote::Process);
mongo::HostAndPort hostAndPort (remote::Process);

/** Launch MongoD/S program on some server in cluster */
remote::Process start (program::Program program);

/** Replica set of mongoD Programs with rs config members and settings to be supplied on startup */
class ReplicaSetProgram {
public:
	std::vector< std::pair<program::Program,mongo::BSONObj> > replicas;  // mongoD + rsconfig member options
	mongo::BSONObj settings;  // rsconfig set-wide settings like heartbeatSleep
	ReplicaSetProgram (std::vector< std::pair<program::Program,mongo::BSONObj> > replicas, mongo::BSONObj settings) : replicas(replicas), settings(settings) {}
	std::string name();  // replica set name gotten from first replica's 'replSet' option.
};

/** Replica set with given program and config options + generated 'replSet' and options filled in by 'mongoD' (if not already supplied). Set-wide settings can also be supplied. See http://www.mongodb.org/display/DOCS/Replica+Set+Configuration for config details. */
ReplicaSetProgram replicaSet (std::vector< std::pair<program::Options, mongo::BSONObj> > mongoDAndMemberOptions, mongo::BSONObj rsSettings = mongo::BSONObj ());

/** Replica set of mongoD processes. RS name and config can be gotten from any replica process */
class ReplicaSetProcess {
public:
	std::vector<remote::Process> replicas;
	ReplicaSetProcess (std::vector<remote::Process> replicas) : replicas(replicas) {}
	std::string name();  // replica set name gotten from first replica's 'replSet' option.
};

/** Launch replica set on some hosts in network. */
ReplicaSetProcess startRS (ReplicaSetProgram);


/** Return a connection to one of the MongoS's if sharded, the "replicaSet" connection if just replicated, or the solo MongoD if just that. Use the supplied arbitrary number to choose amongst choices if necessary */
mongo::DBClientConnection connect (unsigned r);

}

#endif /* MONGO_DEPLOY_H_ */
