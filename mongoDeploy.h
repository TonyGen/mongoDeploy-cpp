/* */

#ifndef MONGO_DEPLOY_H_
#define MONGO_DEPLOY_H_

#include <utility>
#include <vector>
#include <mongo/client/dbclient.h>
#include <remote/remote.h>

namespace mongoDeploy {

/** Prefix for data directory, a number get appended to this, eg. "/data/db" + "1" */
extern std::string mongoDbPathPrefix;
/** Default MongoD config is merged with user supplied config. User config options take precedence */
extern program::Options defaultMongoD;

/** Start mongod program with given options +
 * unique values generated for dbpath and port options if not already supplied +
 * defaultMongoD where not already supplied. */
remote::Process startMongoD (remote::Host, program::Options = program::emptyOptions);

/** Host and port of a mongoD/S process */
std::string hostPort (remote::Process);
mongo::HostAndPort hostAndPort (remote::Process);

/** Server command-line options + member replSetConfig options for a replica in a replica set */
struct RsMemberSpec {
	program::Options opts;
	mongo::BSONObj memberConfig;
	RsMemberSpec (program::Options opts, mongo::BSONObj memberConfig) : opts(opts), memberConfig(memberConfig) {}
};

/** Replica set of mongoD processes. RS name and config can be gotten from any replica process */
class ReplicaSet {
public:
	std::vector<remote::Process> replicas;
	ReplicaSet (std::vector<remote::Process> replicas) : replicas(replicas) {}
	std::string name();  // replica set name gotten from first replica's 'replSet' option.
	/** Start mongod and add it to replica set */
	/** Replica-set name "/" comma-separated hostPorts of active members only */
	std::string nameActiveHosts();
	void addStartReplica (remote::Host, RsMemberSpec);
	/** Remove i'th replica and stop it */
	void removeStopReplica (unsigned i);
};

/** Start replica set on given servers with given config options + generated 'replSet' and options filled in by 'startMongoD' (if not already supplied). Set-wide settings can also be supplied. See http://www.mongodb.org/display/DOCS/Replica+Set+Configuration for config details. */
ReplicaSet startReplicaSet (std::vector<remote::Host>, std::vector<RsMemberSpec>, mongo::BSONObj rsSettings = mongo::BSONObj());

/** Sharding config servers */
class ConfigSet {
public:
	std::vector<remote::Process> cfgServers;
	ConfigSet (std::vector<remote::Process> cfgServers) : cfgServers(cfgServers) {}
};

/** Start config mongoD on each host. 1 or 3 hosts expected */
ConfigSet startConfigSet (std::vector<remote::Host>, program::Options = program::emptyOptions);

/** Default MongoS config is merged with user supplied config. User config options take precedence */
extern program::Options defaultMongoS;

/** Start mongos program with given options +
 * unique values generated for dbpath and port options if not already supplied +
 * defaultMongoS where not already supplied. */
remote::Process startMongoS (remote::Host, ConfigSet, program::Options = program::emptyOptions);

/** A full sharding deployment with routers (mongos), config servers (ConfigSet), and ReplicaSet shards */
class ShardSet {
public:
	std::vector<ReplicaSet> shards;
	ConfigSet configSet;
	std::vector<remote::Process> routers;  // mongos's
	/** ShardSet starts out empty (no shards) */
	ShardSet (ConfigSet configSet, std::vector<remote::Process> routers) : configSet(configSet), routers(routers) {}
	/** Start replica set and add it as another shard */
	void addStartShard (std::vector<remote::Host>, std::vector<RsMemberSpec>, mongo::BSONObj rsSettings = mongo::BSONObj());
	/** Remove i'th shard and stop it */
	void removeStopShard (unsigned i);
	/** Start mongos and add it to available routers */
	void addStartRouter (remote::Host, program::Options = program::emptyOptions);
	/** Remove i'th router from list and stop it */
	void removeStopRouter (unsigned i);
};

/** Start empty shard set with given config server specs and router (mongos) specs */
ShardSet startShardSet (std::vector<remote::Host> cfgHosts, std::vector<remote::Host> routerHosts, program::Options cfgOpts = program::emptyOptions, program::Options routerOpts = program::emptyOptions);


/** Return a connection to one of the MongoS's if sharded, the "replicaSet" connection if just replicated, or the solo MongoD if just that. Use the supplied arbitrary number to choose amongst choices if necessary */
//mongo::DBClientConnection connect (unsigned r);

}

#endif /* MONGO_DEPLOY_H_ */
