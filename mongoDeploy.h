/* Deploy mongo processes on servers */

#ifndef MONGO_DEPLOY_H_
#define MONGO_DEPLOY_H_

#include <utility>
#include <vector>
#include <mongo/client/dbclient.h>
#include <remote/remote.h>
#include <remote/process.h>

namespace mongoDeploy {

/** Prefix for data directory, a number get appended to this, eg. "dbms" + "1" */
extern std::string mongoDbPathPrefix;
/** Default MongoD config is merged with user supplied config. User config options take precedence */
extern program::Options defaultMongoD;

/** Start mongod program with given options +
 * unique values generated for dbpath and port options if not already supplied +
 * defaultMongoD options where not already supplied. */
rprocess::Process startMongoD (remote::Host, program::Options = program::Options());

inline bool isMongoD (rprocess::Process p) {return p.process.program.executable == "mongod";}

/** Host and port of a mongoD/S process */
std::string hostPort (rprocess::Process);
mongo::HostAndPort hostAndPort (rprocess::Process);

/** Server command-line options + member replSetConfig options for a single replica in a replica set */
struct RsMemberSpec {
	program::Options opts;
	mongo::BSONObj memberConfig;
	RsMemberSpec (program::Options opts, mongo::BSONObj memberConfig) : opts(opts), memberConfig(memberConfig) {}
};

/** Replica set of mongoD processes. RS name and config can be gotten from any replica process */
class ReplicaSet {
public:
	std::vector<rprocess::Process> replicas;
	std::vector<RsMemberSpec> memberSpecs;
	ReplicaSet (std::vector<rprocess::Process> replicas, std::vector<RsMemberSpec> memberSpecs) : replicas(replicas), memberSpecs(memberSpecs) {}
	ReplicaSet () {}  // for serialization
	std::string name();  // replica set name gotten from first replica's 'replSet' option.
	/** Active replicas. Excludes arbiter and passive replicas */
	std::vector<rprocess::Process> activeReplicas();
	/** Replica-set name "/" comma-separated hostPorts of active members only */
	std::string nameActiveHosts();
	/** Start mongod and add it to replica set */
	void addStartReplica (remote::Host, RsMemberSpec);
	/** Remove i'th replica and stop it */
	void removeStopReplica (unsigned i);
};

/** Start replica set on given servers with given config options + generated 'replSet' and options filled in by 'startMongoD' (if not already supplied). Set-wide rsSettings can also be supplied. See http://www.mongodb.org/display/DOCS/Replica+Set+Configuration for config details. */
ReplicaSet startReplicaSet (std::vector<remote::Host>, std::vector<RsMemberSpec>, mongo::BSONObj rsSettings = mongo::BSONObj());

/** Sharding config servers */
class ConfigSet {
public:
	std::vector<rprocess::Process> cfgServers;
	ConfigSet (std::vector<rprocess::Process> cfgServers) : cfgServers(cfgServers) {}
	ConfigSet () {}  // for serialization
};

/** Start config mongoD on each host. 1 or 3 hosts expected */
ConfigSet startConfigSet (std::vector<remote::Host>, program::Options = program::Options());

/** Default MongoS config is merged with user supplied config. User config options take precedence */
extern program::Options defaultMongoS;

/** Start mongos program with given options +
 * unique values generated for dbpath and port options if not already supplied +
 * defaultMongoS options where not already supplied. */
rprocess::Process startMongoS (remote::Host, ConfigSet, program::Options = program::Options());

/** A full sharding deployment with routers (mongos), config servers (ConfigSet), and ReplicaSet shards */
class ShardSet {
public:
	ConfigSet configSet;
	std::vector<rprocess::Process> routers;  // mongos's
	std::vector<ReplicaSet> shards;
	/** ShardSet starts out empty (no shards) */
	ShardSet (ConfigSet configSet, std::vector<rprocess::Process> routers) : configSet(configSet), routers(routers) {}
	ShardSet () {}  // for serialization
	/** Start replica set of given specs on given hosts, and add it as another shard */
	void addStartShard (std::vector<remote::Host>, std::vector<RsMemberSpec>, mongo::BSONObj rsSettings = mongo::BSONObj());
	/** Remove i'th shard and stop it */
	void removeStopShard (unsigned i);
	/** Start mongos and add it to available routers */
	void addStartRouter (remote::Host, program::Options = program::Options());
	/** Remove i'th router from list and stop it */
	void removeStopRouter (unsigned i);
};

/** Start empty shard set with given config server specs and router (mongos) specs */
ShardSet startShardSet (std::vector<remote::Host> cfgHosts, std::vector<remote::Host> routerHosts, program::Options cfgOpts = program::Options(), program::Options routerOpts = program::Options());

}

/* Serialization */

#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

namespace boost {
namespace serialization {

template <class Archive> void serialize (Archive & ar, mongoDeploy::ReplicaSet & x, const unsigned version) {
	ar & x.replicas;
}

template <class Archive> void serialize (Archive & ar, mongoDeploy::ConfigSet & x, const unsigned version) {
	ar & x.cfgServers;
}

template <class Archive> void serialize (Archive & ar, mongoDeploy::ShardSet & x, const unsigned version) {
	ar & x.configSet;
	ar & x.routers;
	ar & x.shards;
}

}}

#endif /* MONGO_DEPLOY_H_ */
