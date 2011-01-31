/* */

#include "mongoDeploy.h"
#include <util/util.h>
#include <boost/algorithm/string.hpp>

/** Prefix for data directory, a number get appended to this, eg. "/data/db" + "1" */
std::string mongoDeploy::mongoDbPathPrefix = "/db/db";
/** Default MongoD/S config is merged with user supplied config. User config options take precedence */
program::Options mongoDeploy::defaultMongoD;
program::Options mongoDeploy::defaultMongoS;

unsigned long nextDbPath = 0;

/** mongod program spec with given options +
 * unique values generated for dbpath and port options if not already supplied +.
 * defaultMongoD where not already supplied. */
program::Program mongoDeploy::mongoD (program::Options options) {
	program::Options config;
	config.push_back (std::make_pair (std::string ("rest"), ""));
	config.push_back (std::make_pair (std::string ("dbpath"), mongoDbPathPrefix + to_string (++ nextDbPath)));
	config.push_back (std::make_pair (std::string ("port"), to_string (27017 + nextDbPath)));
	program::Options config1 = program::merge (config, defaultMongoD);
	program::Options config2 = program::merge (config1, options);  //user options have precedence
	program::Program program;
	std::string path = * program::lookup ("dbpath", config2);
	std::stringstream ss;
	ss << "rm -rf " << path << " && mkdir -p " << path;
	program.prepCommand = ss.str();
	program.executable = "mongod";
	program.options = config2;
	return program;
}

/** mongos program spec with given options +
 * unique values generated for dbpath and port options if not already supplied +.
 * defaultMongoS where not already supplied. */
program::Program mongoDeploy::mongoS (program::Options options) {
	program::Options config;
	config.push_back (std::make_pair (std::string ("port"), to_string (27017 + (++ nextDbPath))));
	program::Options config1 = program::merge (config, defaultMongoS);
	program::Options config2 = program::merge (config1, options);  //user options have precedence
	program::Program program;
	program.executable = "mongos";
	program.options = config2;
	return program;
}

/** Host and port of a mongoD/S process */
std::string mongoDeploy::hostPort (remote::Process mongoProcess) {
	remote::Host host = mongoProcess.host;
	std::string port = * program::lookup ("port", mongoProcess.process.program.options);
	return host + ":" + port;
}

mongo::HostAndPort mongoDeploy::hostAndPort (remote::Process mongoProcess) {
	return mongo::HostAndPort (hostPort (mongoProcess));
}

/** Launch MongoD/S program on some server in network */
remote::Process mongoDeploy::start (program::Program program) {
	return remote::launch (cluster::someServer(), program);
}

static unsigned long nextReplicaSetId;

/** Replica set with given program and config options + generated 'replSet' and options filled in by 'mongoD' (if not already supplied) */
mongoDeploy::ReplicaSetProgram mongoDeploy::replicaSet (std::vector< std::pair<program::Options, mongo::BSONObj> > mongoDAndMemberOptions, mongo::BSONObj rsSettings) {
	if (mongoDAndMemberOptions.size() == 0) throw std::runtime_error ("can't create empty replica set");
	program::Options options;
	options.push_back (std::make_pair ("replSet", "rs" + to_string (nextReplicaSetId++)));
	std::vector< std::pair<program::Program, mongo::BSONObj> > replicas;
	for (unsigned i = 0; i < mongoDAndMemberOptions.size(); i++) {
		program::Options options1 = program::merge (options, mongoDAndMemberOptions[i].first);
		program::Program p = mongoD (options1);
		replicas.push_back (make_pair (p, mongoDAndMemberOptions[i].second));
	}
	return ReplicaSetProgram (replicas, rsSettings);
}

/** Replica set program for processes
mongoDeploy::ReplicaSet<program::Program> mongoDeploy::rsProgram (ReplicaSet<network::Process> rs) {
	std::vector<program::Program> ps;
	for (unsigned i = 0; i < rs.replicas.size(); i++)
		ps.push_back (rs.replicas[i].process.program);
	return ps;
} */

std::string parseReplSetName (std::string replSetString) {
	std::vector<std::string> parts;
	boost::split (parts, replSetString, boost::is_any_of ("/"));
	return *parts.begin();
}

/** replica set name */
std::string mongoDeploy::ReplicaSetProgram::name () {
	std::string replSetString = * program::lookup ("replSet", replicas[0].first.options);
	return parseReplSetName (replSetString);
}

std::string mongoDeploy::ReplicaSetProcess::name () {
	std::string replSetString = * program::lookup ("replSet", replicas[0].process.program.options);
	return parseReplSetName (replSetString);
}

/** Launch N MongoD's as a replica set on some hosts in network */
mongoDeploy::ReplicaSetProcess mongoDeploy::startRS (ReplicaSetProgram rs) {
	std::vector <remote::Process> procs;
	mongo::BSONArrayBuilder members;
	for (unsigned i = 0; i < rs.replicas.size(); i++) {
		remote::Process p = start (rs.replicas[i].first);
		procs.push_back (p);
		mongo::BSONObjBuilder obj;
		obj.appendElements (BSON ("_id" << i << "host" << hostPort (p)));
		obj.appendElements (rs.replicas[i].second);
		members.append (obj.done());
	}
	sleep (20);
	mongo::DBClientConnection c;
	c.connect (hostPort (procs[0]));
	mongo::BSONObj rsConfig = BSON ("_id" << rs.name() << "members" << members.arr() << "settings" << rs.settings);
    mongo::BSONObj info;
    std::cout << "replSetInitiate: " << rsConfig << " ->" << std::endl;
    c.runCommand ("admin", BSON ("replSetInitiate" << rsConfig), info);
    std::cout << info << std::endl;
    sleep (45);
    c.runCommand ("admin", BSON ("replSetGetStatus" << 1), info);
    std::cout << "replSetGetStatus -> " << info << std::endl;
    //TODO wait until replica set is initiated
	return ReplicaSetProcess (procs);
}

/** Return a connection to one of the MongoS's if sharded, the "replicaSet" connection if just replicated, or the solo MongoD if just that. Use the supplied arbitrary number to choose amongst choices if necessary */
mongo::DBClientConnection mongoDeploy::connect (unsigned r) {
	//TODO
}
