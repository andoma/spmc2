#pragma once

#define SQL_GET_PLUGIN_BY_ID "SELECT created, userid, betasecret, downloadurl FROM plugin WHERE id=?"

#define SQL_GET_PLUGIN_VERSIONS "SELECT created,version,type,author,downloads,showtime_min_version,title,category,synopsis,description,homepage,pkg_digest,icon_digest,published,comment,status FROM version WHERE plugin_id=?"

#define SQL_GET_ALL "SELECT plugin_id,v.created,version,type,author,downloads,showtime_min_version,title,category,synopsis,description,homepage,pkg_digest,icon_digest,published,comment,status,plugin.betasecret FROM version AS v,plugin WHERE plugin_id = id ORDER BY v.created desc"

#define SQL_CHECK_VERSION "SELECT created FROM version WHERE plugin_id = ? AND version = ?"

#define SQL_INSERT_VERSION "INSERT INTO version (plugin_id,version,type,author,showtime_min_version,title,category,synopsis,description,homepage,pkg_digest,icon_digest,comment,status) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)"
