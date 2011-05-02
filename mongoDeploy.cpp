/* */

#include "mongoDeploy.h"
#include <10util/util.h>
#include <boost/algorithm/string.hpp>

/** Prefix for data directory, a number get appended to this, eg. "dbms" + "1" */
std::string mongoDeploy::mongoDbPathPrefix = "dbms";  // in current directory
/** Default MongoD config is merged with user supplied config. User config options take precedence */
program::Options mongoDeploy::defaultMongoD;

static volatile unsigned long nextDbPath;

/** start mongod program with given options +
 * unique values generated for dbpath and port options if not already supplied +.
 * defaultMongoD options where not already supplied. */
rprocess::Process mongoDeploy::startMongoD (remote::Host host, program::Options options) {
	program::Options config;
	config.push_back (std::make_pair (std::string ("rest"), ""));
	config.push_back (std::make_pair (std::string ("dbpath"), mongoDbPathPrefix + to_string (++ nextDbPath)));
	config.push_back (std::make_pair (std::string ("port"), to_string (27100 + nextDbPath)));
	program::Options config1 = program::merge (config, defaultMongoD);
	program::Options config2 = program::merge (config1, options);  //user options have precedence
	program::Program program;
	std::string path = * program::lookup ("dbpath", config2);
	std::stringstream ss;
	ss << "rm -rf " << path << " && mkdir -p " << path;
	program.prepCommand = ss.str();
	program.executable = "mongod";
	program.options = config2;
	return rprocess::launch (host, program);
}

/** Host and port of a mongoD/S process */
std::string mongoDeploy::hostPortString (rprocess::Process mongoProcess) {
	remote::Host host = mongoProcess.host();
	std::string port = * program::lookup ("port", rprocess::program (mongoProcess) .options);
	return remote::hostPort(host).hostname + ":" + port;
}

mongo::HostAndPort mongoDeploy::hostAndPort (rprocess::Process mongoProcess) {
	return mongo::HostAndPort (hostPortString (mongoProcess));
}

static volatile unsigned long nextReplicaSetId;

/** Start replica set with given member specs and config options + generated 'replSet' and options filled in by 'startMongoD' (if not already supplied) */
mongoDeploy::ReplicaSet mongoDeploy::startReplicaSet (std::vector<remote::Host> hosts, std::vector<RsMemberSpec> memberSpecs, mongo::BSONObj rsSettings) {
	assert (hosts.size() == memberSpecs.size());
	if (memberSpecs.size() == 0) throw std::runtime_error ("can't create empty replica set");
	program::Options options;
	std::string rsName = "rs" + to_string (++ nextReplicaSetId);
	options.push_back (std::make_pair ("replSet", rsName));
	std::vector<rprocess::Process> replicas;
	mongo::BSONArrayBuilder members;
	for (unsigned i = 0; i < min (hosts.size(), memberSpecs.size()); i++) {
		rprocess::Process p = startMongoD (hosts[i], program::merge (options, memberSpecs[i].opts));
		replicas.push_back (p);
		mongo::BSONObjBuilder obj;
		obj.appendElements (BSON ("_id" << i << "host" << hostPortString (p)));
		obj.appendElements (memberSpecs[i].memberConfig);
		members.append (obj.done());
	}
	sleep (32); // TODO: check for liveness instead of assuming it takes at most 30 seconds.
	mongo::DBClientConnection c;
	c.connect (hostPortString (replicas[0]));
	mongo::BSONObj rsConfig = BSON ("_id" << rsName << "members" << members.arr() << "settings" << rsSettings);
    mongo::BSONObj info;
    std::cout << "replSetInitiate: " << rsConfig << " ->" << std::endl;
    c.runCommand ("admin", BSON ("replSetInitiate" << rsConfig), info);
    std::cout << info << std::endl;
    sleep (45); // TODO: check for readiness instead of assuming it takes at most 45 seconds.
    c.runCommand ("admin", BSON ("replSetGetStatus" << 1), info);
    std::cout << "replSetGetStatus -> " << info << std::endl;
    //TODO wait until replica set is initiated
	return ReplicaSet (replicas, memberSpecs);
}

/** Extract replica set name from replica set name + seed list */
static std::string parseReplSetName (std::string replSetString) {
	std::vector<std::string> parts;
	boost::split (parts, replSetString, boost::is_any_of ("/"));
	return *parts.begin();
}

std::string mongoDeploy::ReplicaSet::name () {
	std::string replSetString = * program::lookup ("replSet", rprocess::program (replicas[0]) .options);
	return parseReplSetName (replSetString);
}

/** Active (non-arbiter, non-passive) replicas in replica set */
// TODO: exclude passive too
std::vector <rprocess::Process> mongoDeploy::ReplicaSet::activeReplicas () {
	assert (replicas.size() == memberSpecs.size());
	std::vector <rprocess::Process> active;
	for (unsigned i = 0; i < replicas.size(); i++) {
		if (! memberSpecs[i].memberConfig.getBoolField ("arbiterOnly"))
			active.push_back (replicas[i]);
	}
	return active;
}

/** Replica-set name "/" comma-separated hostPorts of active hosts.
 * Passive hosts are not included because it breaks addshard command */
std::string mongoDeploy::ReplicaSet::nameActiveHosts () {
	return name() + "/" + concat (intersperse (std::string(","), fmap (hostPortString, activeReplicas())));
}

static void addReplica (mongoDeploy::ReplicaSet rs, rprocess::Process mongod, mongo::BSONObj memberConfig) {
	vector<mongo::HostAndPort> hs = fmap (mongoDeploy::hostAndPort, rs.replicas);
	mongo::DBClientReplicaSet c (rs.name(), hs);
	bool ok = c.connect();
	if (!ok) throw std::runtime_error ("Unable to connect to replica set " + rs.name());

	mongo::BSONObj cfg = c.findOne ("local.system.replset", mongo::BSONObj());
	if (cfg.isEmpty()) throw std::runtime_error ("Missing replica set config " + rs.name());
	int ver = cfg.getIntField ("version");
	cfg = cfg.replaceFieldNames (BSON ("version" << ver+1));
	//TODO: finish this method
}

/** Start mongod and add it to replica set */
void mongoDeploy::ReplicaSet::addStartReplica (remote::Host host, RsMemberSpec memberSpec) {
	program::Options options;
	options.push_back (std::make_pair ("replSet", name()));
	rprocess::Process proc = startMongoD (host, program::merge (options, memberSpec.opts));
	sleep (20);  //TODO: check for liveness instead of assuming live after 20 secs
	addReplica (*this, proc, memberSpec.memberConfig);
}

/** Remove i'th replica and stop it */
void removeStopReplica (unsigned i) {
	//TODO
}

/** Start config mongoD on each host. 1 or 3 hosts expected */
mongoDeploy::ConfigSet mongoDeploy::startConfigSet (std::vector<remote::Host> hosts, program::Options opts) {
	std::vector<rprocess::Process> procs;
	for (unsigned i = 0; i < hosts.size(); i++) {
		rprocess::Process p = startMongoD (hosts[i], opts);
		procs.push_back (p);
	}
	sleep (20);  //TODO: check for liveness instead of assuming live after 20 secs
	return ConfigSet (procs);
}

/** Default MongoD config is merged with user supplied config. User config options take precedence */
program::Options mongoDeploy::defaultMongoS;

/** Start mongos program with given options +
 * unique values generated for dbpath and port options if not already supplied +.
 * defaultMongoS where not already supplied. */
rprocess::Process mongoDeploy::startMongoS (remote::Host host, ConfigSet cs, program::Options options) {
	program::Options config;
	config.push_back (std::make_pair (std::string ("port"), to_string (27100 + (++ nextDbPath))));
	config.push_back (std::make_pair (std::string ("configdb"), concat (intersperse (std::string(","), fmap (hostPortString, cs.cfgServers)))));
	program::Options config1 = program::merge (config, defaultMongoS);
	program::Options config2 = program::merge (config1, options);  //user options have precedence
	program::Program program;
	program.executable = "mongos";
	program.options = config2;
	return rprocess::launch (host, program);
}

/** Start empty shard set with given config server specs and router (mongos) specs */
mongoDeploy::ShardSet mongoDeploy::startShardSet (std::vector<remote::Host> cfgHosts, std::vector<remote::Host> routerHosts, program::Options cfgOpts, program::Options routerOpts) {
	ConfigSet cs = startConfigSet (cfgHosts, cfgOpts);
	boost::function1<rprocess::Process,remote::Host> f = boost::bind (startMongoS, _1, cs, routerOpts);
	std::vector<rprocess::Process> rs = fmap (f, routerHosts);
	return ShardSet (cs, rs);
}

static void addShard (mongoDeploy::ShardSet& s, mongoDeploy::ReplicaSet r) {
	mongo::DBClientConnection c;
	c.connect (mongoDeploy::hostPortString (s.routers[0]));
    mongo::BSONObj info;
    mongo::BSONObj cmd = BSON ("addshard" << r.nameActiveHosts());
    std::cout << cmd << " -> " << std::endl;
    c.runCommand ("admin", cmd, info);
    std::cout << info << std::endl;
    c.runCommand ("admin", BSON ("listshards" << 1), info);
    std::cout << "listshards -> " << info << std::endl;
    s.shards.push_back (r);
}

/** Start replica set of given specs on given hosts, and add it as another shard */
void mongoDeploy::ShardSet::addStartShard (std::vector<remote::Host> hosts, std::vector<RsMemberSpec> memberSpec, mongo::BSONObj rsSettings) {
	ReplicaSet r = startReplicaSet (hosts, memberSpec, rsSettings);
	addShard (*this, r);
}

/** Remove i'th shard and stop it */
void mongoDeploy::ShardSet::removeStopShard (unsigned i) {
	//TODO
}

/** Start mongos and add it to available routers */
void mongoDeploy::ShardSet::addStartRouter (remote::Host host, program::Options opts) {
	//TODO
}

/** Remove i'th router from list and stop it */
void mongoDeploy::ShardSet::removeStopRouter (unsigned i) {
	//TODO
}
