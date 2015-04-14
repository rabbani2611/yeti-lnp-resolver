#include "resolver.h"
#include "cfg.h"
#include "cache.h"
#include "resolver_driver_sip.h"
#include "resolver_driver_http_thinq.h"

#include <pqxx/pqxx>

#define LOAD_LNP_STMT "SELECT * FROM load_lnp_databases()"

_resolver::_resolver()
{}

_resolver::~_resolver()
{}

static inline resolver_driver *create_resolver_driver(const resolver_driver::driver_cfg &dcfg)
{
	resolver_driver *d;
	switch(dcfg.driver_id){
	case RESOLVER_DRIVER_SIP: d = new resolver_driver_sip(dcfg); break;
	case RESOLVER_DRIVER_HTTP_THINQ: d = new resolver_driver_http_thinq(dcfg); break;
	default:
		err("unsupported driver_id: %d",dcfg.driver_id);
		return NULL;
	}
	if(!d) {
		err("can't load resolve driver %d");
	}
	return d;
}

void _resolver::stop()
{
	for(auto &i: databases)
		i.second->driver->on_stop();
}

void _resolver::launch()
{
	for(const auto &it: databases) {
		const database_entry &e = *it.second;
		e.driver->launch();
	}
}

bool _resolver::load_resolve_drivers(databases_t &db)
{
	//load from database & init resolvers map
	bool ret = false;

	try {
		pqxx::result r;
		pqxx::connection c(cfg.db.get_conn_string());
		c.set_variable("search_path",cfg.db.schema+", public");

		pqxx::work t(c);
		r = t.exec(LOAD_LNP_STMT);
		for(pqxx::result::size_type i = 0; i < r.size();++i){
			const pqxx::result::tuple &row = r[i];
			resolver_driver::driver_cfg dcfg(row);
			resolver_driver *d =
				create_resolver_driver(dcfg);
			if(!d) continue;

//check for associative containers emplace() method support
#if __GNUC__ >= 4 && __GNUC_MINOR__ >=8 //check for gcc4.8
			db.emplace(dcfg.database_id,
							  std::unique_ptr<database_entry>(
								  new database_entry(dcfg.name,d))
							  );
#else
			db.insert(std::pair<int,std::unique_ptr<database_entry> >(
						  dcfg.database_id,
						  std::unique_ptr<database_entry>(
							  new database_entry(dcfg.name,d))
						  )
					  );
#endif
		}
		info("loaded %ld databases",db.size());
		for(const auto &i : db)
			i.second->driver->show_info(i.first);

		ret = true;
	} catch(const pqxx::pqxx_exception &e){
		err("pqxx_exception: %s ",e.base().what());
	} catch(...){
		err("unexpected exception");
	}
	return ret;
}

bool _resolver::configure()
{
	databases_t tdb;
	bool ret = load_resolve_drivers(tdb);
	if(!ret) goto out;

	databases_m.lock();

	databases.swap(tdb);
	try {
		launch();
	} catch(...){ }

	databases_m.unlock();
out:
	for(auto &i: tdb)
		i.second->driver->on_stop();
	tdb.clear();
	return ret;
}

void _resolver::resolve(int database_id, const string &in, string &out)
{
	guard(databases_m);

	auto i = databases.find(database_id);
	if(i==databases.end()){
		throw resolve_exception(1,"uknown database id");
	}

	string data;
	try {
		i->second->driver->resolve(in,out,data);
		dbg("resolved: %s -> %s using database %d",
			in.c_str(),out.c_str(),database_id);
	} catch(resolve_exception &e){
		throw e;
	} catch(...){
		dbg("unknown resolve exeption");
		throw resolve_exception(1,"internal error");
	}
	lnp_cache::instance()->sync(new cache_entry(database_id,in,out,data));
}

