#include "first.h"

#include "base.h"
#include "buffer.h"
#include "log.h"
#include "plugin.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "dp_util.h"

#define ENTERLOG DEBUG_ERRPRINT("enter\n");

/* plugin config for all request/connections */
typedef struct {
	buffer *cgidir;
} plugin_config;

/*! @name API for loading conf file
/* @{ */
static void mod_loopcgi_conf_init(plugin_config *);
static void mod_loopcgi_conf_free(plugin_config *);
static void mod_loopcgi_get_cgipath(array * alias, buffer * cgidir);
static int mod_loopcgi_patch_connection(server *srv, data_config const* dc, config_values_t *cv, config_values_t *cv_tmp, config_scope_type_t scope);
/* @} */

#define LOOPCGI_CGIPREFIX "/cgi-bin"
#define LOOPCGI_DEFALUT_INTERVAL (1)
#define LOOPCGI_DEFAULT_LOOPCOUNT (-1)

typedef struct mod_loopcgi_req_setting_t {
	int interval;//interval (sec)
	int count_max;// set count max if query has count, negative value=> endress
	char command[256];//command
} mod_loopcgi_req_setting_t;

typedef struct mod_loopcgi_con_info_t *LOOPCGI_CON_INFO;
struct mod_loopcgi_con_info_t {
	LOOPCGI_CON_INFO next;
	LOOPCGI_CON_INFO prev;
	connection *con;//set con pointer only for checking connection is same or not
	int current_count;//current count
	mod_loopcgi_req_setting_t setting;
};

typedef struct mod_loopcgi_request_info_t {
	LOOPCGI_CON_INFO head;
	LOOPCGI_CON_INFO tail;
} mod_loopcgi_request_info_t;

#define mod_loopcgi_connection_push(this, data) dputil_list_push((DPUtilList)(this), (DPUtilListData)(data))
#define mod_loopcgi_connection_pull(this, data) dputil_list_pull((DPUtilList)(this), (DPUtilListData)(data))
#define mod_loopcgi_connection_pop(this) (LOOPCGI_CON_INFO)dputil_list_pop((DPUtilList)(this))

/*! @name API for LOOPCGI_CON_INFO*/
/* @{ */
static LOOPCGI_CON_INFO mod_loopcgi_con_info_new(connection *con, mod_loopcgi_req_setting_t *req_setting);
static int mod_loopcgi_con_info_call_once(LOOPCGI_CON_INFO this, connection *con);
static int mod_loopcgi_con_info_wait(LOOPCGI_CON_INFO this, connection *con);
static handler_t mod_loopcgi_con_info_start(LOOPCGI_CON_INFO this, connection *con);
static void mod_loopcgi_con_info_free(LOOPCGI_CON_INFO);

/* @} */

/*! @name API for mod_loopcgi_request_info_t*/
/* @{ */
static LOOPCGI_CON_INFO mod_loopcgi_get_coninfo(connection *con, mod_loopcgi_request_info_t *req_info);
static void mod_loopcgi_request_info_free(mod_loopcgi_request_info_t * req_info);
/* @} */

/*plugin data*/
typedef struct {
	PLUGIN_DATA;
	plugin_config conf;
	mod_loopcgi_request_info_t request_info;
} plugin_data;

/*! @name definition to check this plugin request */
/* @{ */
#define LOOPCGI_ACCEPT_URL "/loopcgi"

static const char * support_queries[] = {
	"cgi",/*cgi command*/
	"interval",/*polling interval, sec*/
	"count",/*loop count*/
};
enum {
	INDEX_CGI,
	INDEX_INTERVAL,
	INDEX_COUNT,
};

typedef struct parse_query_t {
	const char * query_key;
	const char * value;
} parse_query_t;

/* API to parse queries. Please set "key" part, it will parse queries and set "value" if it has*/
static void mod_loopcgi_parse_query(const char *request_query, parse_query_t *input_queries);
static void mod_loopcgi_parse_queries(connection *con, parse_query_t *queries);
static inline int mod_loopcgi_is_ownreq( buffer * req_uri, const char * accept_uri);
static inline handler_t mod_loopcgi_failed(connection *con, int http_status);
static inline handler_t mod_loopcgi_finished(connection *con, int http_status);

//for this plugin
static int mod_loopcgi_set_command(plugin_config *conf,  const char *cginame, mod_loopcgi_req_setting_t * setting);
/* @} */

/**********/
/*! @name implement API for loading conf file
/* @{ */
static void mod_loopcgi_conf_init(plugin_config *conf) {
	conf->cgidir = buffer_init();
}

static void mod_loopcgi_conf_free(plugin_config *conf) {
	buffer_free(conf->cgidir);
}

static void mod_loopcgi_get_cgipath(array * alias, buffer * cgidir) {
	int i=0;
	//search "cgi-bin" path
	for (i = 0; i < alias->used; i++) {
		data_string *ds = (data_string *)alias->data[i];

		if (buffer_is_empty(ds->key) || buffer_is_empty(ds->value)) continue;

		DEBUG_ERRPRINT("check key %s\n", ds->key->ptr);
		int alias_len = buffer_string_length(ds->key);
		if (0 != strncasecmp(LOOPCGI_CGIPREFIX, ds->key->ptr, alias_len)) continue;

		//copy cgi path
		DEBUG_ERRPRINT("copy dir name %s\n", ds->value->ptr);
		buffer_copy_buffer(cgidir, ds->value);
		break;
	}
}

static int mod_loopcgi_patch_connection(server *srv, data_config const* dc, config_values_t *cv, config_values_t *cv_tmp, config_scope_type_t scope) {
	int i=0, cv_index=0, cv_tmp_index=0;
	/* search conf type */
	for (i = 0; i < dc->value->used; i++) {
	        data_unset *du = dc->value->data[i];

		DEBUG_ERRPRINT("check key %s\n", du->key->ptr);
		for(cv_index=0, cv_tmp_index=0; cv[cv_index].key; cv_index++) {
			DEBUG_ERRPRINT("compair %s and %s\n", du->key->ptr, cv[cv_index].key);
			DEBUG_ERRPRINT("(buffer size=%d\n", (int)du->key->used);
			if (buffer_is_equal_string(du->key, cv[cv_index].key, strlen(cv[cv_index].key))) {
				//add to tmpcv
				DEBUG_ERRPRINT("copy cv to get conf data %s\n", du->key->ptr);
				memcpy(&cv_tmp[cv_tmp_index++], &cv[cv_index], sizeof(config_values_t));
			}
		}
		//add NULL
		cv_tmp[cv_tmp_index]=cv[cv_index];

		//load settings
		if (0 != config_insert_values_global(srv, dc->value, cv_tmp, scope)) {
			return -1;
		}
	}

	return 0;
}
/* @} */

/*! @name implement API for LOOPCGI_CON_INFO*/
/* @{ */
static LOOPCGI_CON_INFO mod_loopcgi_con_info_new(connection *con, mod_loopcgi_req_setting_t *req_setting) {
	LOOPCGI_CON_INFO  info = calloc(1, sizeof(*info));
	info->con = con;
	memcpy(&info->setting, req_setting, sizeof(info->setting));
	return info;
}

static int mod_loopcgi_con_info_call_once(LOOPCGI_CON_INFO this, connection *con) {
	FILE * fp = popen(this->setting.command, "r");
	int ret=-1;

	if(!fp) return -1;

	//buffer to store response
	buffer *b = buffer_init();

	char result_buf[256]={0};
	char * result;
	while(fgets(result_buf, sizeof(result_buf), fp) != NULL) {
		buffer_append_string_len(b, result_buf, strlen(result_buf));
	}

	//if there is a response, add information to chunk
	if(!buffer_is_empty(b)) {
		chunkqueue_append_buffer(con->write_queue, b);
		ret = 0;
	}

	buffer_free(b);
	pclose(fp);
	this->current_count++;
	return ret;
}

static inline int mod_loopcgi_con_info_is_finished(LOOPCGI_CON_INFO this) {
	return (this->setting.count_max != LOOPCGI_DEFAULT_LOOPCOUNT) && (this->setting.count_max <= this->current_count);
}

static handler_t mod_loopcgi_con_info_start(LOOPCGI_CON_INFO this, connection *con) {
	//loop calling command
	while(1) {
		if(mod_loopcgi_con_info_call_once(this, con)) {
			//failed to call
			return mod_loopcgi_failed(con, 500);
		}

		if(mod_loopcgi_con_info_is_finished(this)) break;

		sleep(this->setting.interval);
	}
	return mod_loopcgi_finished(con, 200);
}

static void mod_loopcgi_con_info_free(LOOPCGI_CON_INFO this) {
	free(this);
}
/* @} */

/*! @name implement API for mod_loopcgi_request_info_t*/
/* @{ */
static LOOPCGI_CON_INFO mod_loopcgi_get_coninfo(connection *con, mod_loopcgi_request_info_t *req_info) {
	LOOPCGI_CON_INFO info=req_info->head;
	while(info) {
		if(info->con==con) break;
		info=info->next;
	}
	return info;
}

static void mod_loopcgi_request_info_free(mod_loopcgi_request_info_t * req_info) {
	LOOPCGI_CON_INFO info = mod_loopcgi_connection_pop(req_info);
	while(info) {
		mod_loopcgi_con_info_free(info);
		info = mod_loopcgi_connection_pop(req_info);
	}
}
/* @} */

static void mod_loopcgi_parse_query(const char *request_query, parse_query_t *input_queries) {
	char *query_val;
	int input_query_len, request_query_len;
	parse_query_t * input_query;
	for(input_query = input_queries; input_query->query_key; input_query++) {
		DEBUG_ERRPRINT("check query key %s\n", input_query->query_key);
		query_val = strstr(request_query, "=");
		if(!query_val) continue;

		DEBUG_ERRPRINT("request query key %s\n", request_query);
		//check query
		input_query_len=strlen(input_query->query_key);
		request_query_len=(int)(query_val - request_query);
		if(input_query_len != request_query_len || memcmp(input_query->query_key, request_query, input_query_len) != 0) continue;

		//set value pointer
		DEBUG_ERRPRINT("set query value %s\n", query_val+1);
		input_query->value = query_val+1;
	}
}

static void mod_loopcgi_parse_queries(connection *con, parse_query_t *queries) {
	if(buffer_string_is_empty(con->uri.query)) {
		return;
	}

	//parse query
	int input_query_index=0;
	char *query_val;
	char *query = strtok( con->uri.query->ptr, "&" );
	while(query) {
		mod_loopcgi_parse_query(query, queries);

		//goto next
		query = strtok(NULL, "&" );
	}
}

static inline int mod_loopcgi_is_ownreq( buffer * req_uri, const char * accept_uri) {
	size_t accept_uri_len = strlen(accept_uri);
	size_t req_uri_len = buffer_string_length(req_uri);

	//check head of uri is same or not
	if((req_uri_len < accept_uri_len) || (strncmp(req_uri->ptr, accept_uri, accept_uri_len)!=0)) {
		return 0;
	}

	//same URL perfectly, or req_uri has query "?" => OK
	return (accept_uri_len == req_uri_len) || (req_uri->ptr[accept_uri_len]=='?');
}

static inline handler_t mod_loopcgi_failed(connection *con, int http_status) {
	con->http_status = http_status;
	con->mode = DIRECT;
	return HANDLER_FINISHED;
}

static inline handler_t mod_loopcgi_finished(connection *con, int http_status) {
	con->http_status = http_status;
	con->file_finished = 1;
	return HANDLER_FINISHED;
}

static int mod_loopcgi_set_command(plugin_config *conf,  const char *cginame, mod_loopcgi_req_setting_t * setting) {
	int i=0;
	//call help
	char helpcmd[256]={0};
	char *help_pt = helpcmd;

	//move command
	help_pt += sprintf(help_pt, "cd ");
	memcpy(help_pt, conf->cgidir->ptr, conf->cgidir->used - 1);
	help_pt += conf->cgidir->used-1;

	//add ";"
	*help_pt=';';
	help_pt++;

	//only support python
	help_pt += sprintf(help_pt, "/usr/bin/python3.6 ");

	//get cgi command name
	char * cgicmd_endprefix = strstr(cginame, "&");
	int namelen;
	if(cgicmd_endprefix) {
		namelen = strlen(cginame)-strlen(cgicmd_endprefix);
	} else {
		namelen = strlen(cginame);
	}
	memcpy(help_pt, cginame, namelen);
	help_pt+=namelen;

	//get linux command from cgi help
	help_pt+=sprintf(help_pt, " --help |  awk -F\",\" '{for(i=2;i<=NF;i++)printf $i \" \"}'");

	DEBUG_ERRPRINT("call help! %s\n", helpcmd);

	FILE * fp = popen(helpcmd, "r");
	if(!fp) return -1;

	char * result = fgets(setting->command, sizeof(setting->command), fp);
	pclose(fp);
	DEBUG_ERRPRINT("use command:[%s]\n", setting->command);
	return (result)?0:-1;
}

/**********/
/*! @name plugin API implement */
/* @{ */
/* init the plugin data */
INIT_FUNC(mod_loopcgi_init) {
ENTERLOG
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_loopcgi_free) {
ENTERLOG
	UNUSED(srv);
	plugin_data *p = p_d;

	if (!p) return HANDLER_GO_ON;

	//free conf
	mod_loopcgi_conf_free(&p->conf);

	//free request info
	mod_loopcgi_request_info_free(&p->request_info);
	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */
SETDEFAULTS_FUNC(mod_loopcgi_set_defaults) {
ENTERLOG
	plugin_data *p = p_d;

	if (!p) return HANDLER_ERROR;

	config_values_t cv[] = {
		{ "alias.url",                  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },/* 0, to load cgi dir */
		{ NULL,                         NULL, T_CONFIG_UNSET,  T_CONFIG_SCOPE_UNSET }
	};

	//set data to load conf
	array * alias = array_init();
	cv[0].destination = alias;

	//create tmp cv
	config_values_t *cv_tmp = calloc(sizeof(cv)/sizeof(cv[0]), sizeof(config_values_t));

	//initalize memory
	mod_loopcgi_conf_init(&p->conf);

	size_t i = 0;
	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];

		//patch connection first
		if(mod_loopcgi_patch_connection(srv, config, cv, cv_tmp, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			log_error_write(srv, __FILE__, __LINE__, "s",
					"unexpected value for mod_loopcgi config;");
		}
	}

	//parse alias
	mod_loopcgi_get_cgipath(alias, p->conf.cgidir);

	//free local info
	array_free(alias);
	free(cv_tmp);

	return HANDLER_GO_ON;
}

URIHANDLER_FUNC(mod_loopcgi_uri_handler) {
ENTERLOG
	UNUSED(srv);
	plugin_data *p = p_d;

	//check request url, if request uri is different to accept_uri, skip
	if (!mod_loopcgi_is_ownreq(con->request.uri, LOOPCGI_ACCEPT_URL)) return HANDLER_GO_ON;

	//parse query
	parse_query_t queries[] = {
		{support_queries[INDEX_CGI], NULL},//cgi
		{support_queries[INDEX_INTERVAL], NULL},//interval
		{support_queries[INDEX_COUNT], NULL},//count
		{NULL, NULL},//count
	};

	mod_loopcgi_parse_queries(con, queries);

	DEBUG_ERRPRINT("check cgi\n");
	//check cgi
	if(!queries[INDEX_CGI].value) {
		DEBUG_ERRPRINT("error, There is no cgi query\n");
                log_error_write(srv, __FILE__, __LINE__, "s", "There is no cgi query");
		return mod_loopcgi_failed(con, 400);
	}

	mod_loopcgi_req_setting_t setting;
	//set cgi command
	if(mod_loopcgi_set_command(&p->conf, queries[INDEX_CGI].value, &setting)) {
		DEBUG_ERRPRINT("error,  Filed to set command\n");
                log_error_write(srv, __FILE__, __LINE__, "s", "Filed to set command");
		return mod_loopcgi_failed(con, 400);
	}

	//set interval
	setting.interval=LOOPCGI_DEFALUT_INTERVAL;
	if(queries[INDEX_INTERVAL].value) {
		int tmp_interval=atoi(queries[INDEX_INTERVAL].value);

		//only accept positive interval
		if(0 < tmp_interval) setting.interval = tmp_interval;
	}

	//set count
	setting.count_max= LOOPCGI_DEFAULT_LOOPCOUNT;
	if(queries[INDEX_COUNT].value) setting.count_max=atoi(queries[INDEX_COUNT].value);

	LOOPCGI_CON_INFO instance = mod_loopcgi_con_info_new(con, &setting);
	mod_loopcgi_connection_push(&p->request_info, instance);

	DEBUG_ERRPRINT("call command\n");
	return mod_loopcgi_con_info_start(instance, con);
}

REQUESTDONE_FUNC(mod_loopcgi_request_done) {
ENTERLOG
	UNUSED(srv);
	plugin_data *p = p_d;
	LOOPCGI_CON_INFO info=mod_loopcgi_get_coninfo(con, &p->request_info);
	if(info) {
		mod_loopcgi_connection_pull(&p->request_info, info);
		mod_loopcgi_con_info_free(info);
	}
	return HANDLER_GO_ON;
}

REQUESTDONE_FUNC(mod_loopcgi_close) {
ENTERLOG
	return mod_loopcgi_request_done(srv, con, p_d);
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_loopcgi_plugin_init(plugin *p);
int mod_loopcgi_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("loopcgi");

	p->init           = mod_loopcgi_init;
	p->set_defaults   = mod_loopcgi_set_defaults;
	p->handle_uri_clean = mod_loopcgi_uri_handler;
	p->cleanup        = mod_loopcgi_free;
	p->handle_connection_close = mod_loopcgi_close;
	p->handle_request_done = mod_loopcgi_request_done;
	p->data        = NULL;

	return 0;
}
