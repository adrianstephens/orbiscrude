#ifndef DMAP_H
#define DMAP_H

#include "base/defs.h"
#include "base/bits.h"
#include "base/algorithm.h"
#include "base/strings.h"

namespace iso {

enum DMAP_TYPE {
	DMAP_UNKNOWN,
	DMAP_UINT,
	DMAP_INT,
	DMAP_STR,
	DMAP_DATA,
	DMAP_DATE,
	DMAP_VERS,
	DMAP_DICT,
	DMAP_ITEM
};

struct dmap_field {
	uint32		code;			// The four-character code used in the encoded message.
	DMAP_TYPE	type;			// The type of data associated with the content code.
	DMAP_TYPE	list_item_type;	// For listings, the type of their listing item children
	const char *name;			// A human-readable name for the content code.
};

static const dmap_field dmap_fields[] = {
	{ "meia"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.itemdateadded" },
	{ "meip"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.itemdateplayed" },
	{ "mext"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.objectextradata" },
	{ "miid"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.itemid" },
	{ "mikd"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.itemkind" },
	{ "abal"_u32,	DMAP_DICT, DMAP_STR,		"daap.browsealbumlisting" },
	{ "abar"_u32,	DMAP_DICT, DMAP_STR,		"daap.browseartistlisting" },
	{ "abcp"_u32,	DMAP_DICT, DMAP_STR,		"daap.browsecomposerlisting" },
	{ "abgn"_u32,	DMAP_DICT, DMAP_STR,		"daap.browsegenrelisting" },
	{ "abpl"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.baseplaylist" },
	{ "abro"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"daap.databasebrowse" },
	{ "adbs"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"daap.databasesongs" },
	{ "aeAD"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"com.apple.itunes.adam-ids-array" },
	{ "aeAI"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.itms-artistid" },
	{ "aeCD"_u32,	DMAP_DATA, DMAP_UNKNOWN,	"com.apple.itunes.flat-chapter-data" },
	{ "aeCF"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.cloud-flavor-id" },
	{ "aeCI"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.itms-composerid" },
	{ "aeCK"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.cloud-library-kind" },
	{ "aeCM"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.cloud-match-type" },
	{ "aeCR"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.content-rating" } ,
	{ "aeCS"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.artworkchecksum" },
	{ "aeCU"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.cloud-user-id" },
	{ "aeCd"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.cloud-id" },
	{ "aeDE"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.longest-content-description" },
	{ "aeDL"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.drm-downloader-user-id" },
	{ "aeDP"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.drm-platform-id" },
	{ "aeDR"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.drm-user-id" },
	{ "aeDV"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.drm-versions" },
	{ "aeEN"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.episode-num-str" },
	{ "aeES"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.episode-sort" },
	{ "aeFA"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.drm-family-id" },
	{ "aeGD"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.gapless-enc-dr" } ,
	{ "aeGE"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.gapless-enc-del" },
	{ "aeGH"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.gapless-heur" },
	{ "aeGI"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.itms-genreid" },
	{ "aeGR"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.gapless-resy" },
	{ "aeGU"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.gapless-dur" },
	{ "aeGs"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.can-be-genius-seed" },
	{ "aeHC"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.has-chapter-data" },
	{ "aeHD"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.is-hd-video" },
	{ "aeHV"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.has-video" },
	{ "aeK1"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.drm-key1-id" },
	{ "aeK2"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.drm-key2-id" },
	{ "aeMC"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.playlist-contains-media-type-count" },
	{ "aeMK"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.mediakind" },
	{ "aeMX"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.movie-info-xml" },
	{ "aeMk"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.extended-media-kind" },
	{ "aeND"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.non-drm-user-id" },
	{ "aeNN"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.network-name" },
	{ "aeNV"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.norm-volume" },
	{ "aePC"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.is-podcast" },
	{ "aePI"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.itms-playlistid" },
	{ "aePP"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.is-podcast-playlist" },
	{ "aePS"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.special-playlist" },
	{ "aeRD"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.rental-duration" },
	{ "aeRP"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.rental-pb-start" },
	{ "aeRS"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.rental-start" },
	{ "aeRU"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.rental-pb-duration" },
	{ "aeRf"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.is-featured" },
	{ "aeSE"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store-pers-id" },
	{ "aeSF"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.itms-storefrontid" },
	{ "aeSG"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.saved-genius" },
	{ "aeSI"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.itms-songid" },
	{ "aeSN"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.series-name" },
	{ "aeSP"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.smart-playlist" },
	{ "aeSU"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.season-num" },
	{ "aeSV"_u32,	DMAP_VERS, DMAP_UNKNOWN,	"com.apple.itunes.music-sharing-version" },
	{ "aeXD"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.xid" },
	{ "aecp"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.collection-description" },
	{ "aels"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.liked-state" },
	{ "aemi"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"com.apple.itunes.media-kind-listing-item" },
	{ "aeml"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"com.apple.itunes.media-kind-listing" },
	{ "agac"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.groupalbumcount" },
	{ "agma"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.groupmatchedqueryalbumcount" },
	{ "agmi"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.groupmatchedqueryitemcount" },
	{ "agrp"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songgrouping" },
	{ "ajAE"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store.ams-episode-type" },
	{ "ajAS"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store.ams-episode-sort-order" },
	{ "ajAT"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store.ams-show-type" },
	{ "ajAV"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store.is-ams-video" },
	{ "ajal"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store.album-liked-state" },
	{ "ajcA"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store.show-composer-as-artist" },
	{ "ajca"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store.show-composer-as-artist" },
	{ "ajuw"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.store.use-work-name-as-display-name" },
	{ "amvc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songmovementcount" },
	{ "amvm"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songmovementname" },
	{ "amvn"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songmovementnumber" },
	{ "aply"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"daap.databaseplaylists" },
	{ "aprm"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.playlistrepeatmode" },
	{ "apro"_u32,	DMAP_VERS, DMAP_UNKNOWN,	"daap.protocolversion" },
	{ "apsm"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.playlistshufflemode" },
	{ "apso"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"daap.playlistsongs" },
	{ "arif"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"daap.resolveinfo" },
	{ "arsv"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"daap.resolve" },
	{ "asaa"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songalbumartist" },
	{ "asac"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songartworkcount" },
	{ "asai"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songalbumid" },
	{ "asal"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songalbum" },
	{ "asar"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songartist" },
	{ "asas"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songalbumuserratingstatus" },
	{ "asbk"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.bookmarkable" },
	{ "asbo"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songbookmark" },
	{ "asbr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songbitrate" },
	{ "asbt"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songbeatsperminute" },
	{ "ascd"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songcodectype" },
	{ "ascm"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songcomment" },
	{ "ascn"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songcontentdescription" },
	{ "asco"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songcompilation" },
	{ "ascp"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songcomposer" },
	{ "ascr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songcontentrating" },
	{ "ascs"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songcodecsubtype" },
	{ "asct"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songcategory" },
	{ "asda"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"daap.songdateadded" },
	{ "asdb"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songdisabled" },
	{ "asdc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songdisccount" },
	{ "asdk"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songdatakind" },
	{ "asdm"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"daap.songdatemodified" },
	{ "asdn"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songdiscnumber" },
	{ "asdp"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"daap.songdatepurchased" },
	{ "asdr"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"daap.songdatereleased" },
	{ "asdt"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songdescription" },
	{ "ased"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songextradata" },
	{ "aseq"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songeqpreset" },
	{ "ases"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songexcludefromshuffle" },
	{ "asfm"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songformat" },
	{ "asgn"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songgenre" },
	{ "asgp"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songgapless" },
	{ "asgr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.supportsgroups" },
	{ "ashp"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songhasbeenplayed" },
	{ "askd"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"daap.songlastskipdate" },
	{ "askp"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songuserskipcount" },
	{ "asky"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songkeywords" },
	{ "aslc"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songlongcontentdescription" },
	{ "aslr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songalbumuserrating" },
	{ "asls"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songlongsize" },
	{ "aspc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songuserplaycount" },
	{ "aspl"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"daap.songdateplayed" },
	{ "aspu"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songpodcasturl" },
	{ "asri"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songartistid" },
	{ "asrs"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songuserratingstatus" },
	{ "asrv"_u32,	DMAP_INT,  DMAP_UNKNOWN,	"daap.songrelativevolume" },
	{ "assa"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.sortartist" },
	{ "assc"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.sortcomposer" },
	{ "assl"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.sortalbumartist" },
	{ "assn"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.sortname" },
	{ "assp"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songstoptime" },
	{ "assr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songsamplerate" },
	{ "asss"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.sortseriesname" },
	{ "asst"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songstarttime" },
	{ "assu"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.sortalbum" },
	{ "assz"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songsize" },
	{ "astc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songtrackcount" },
	{ "astm"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songtime" },
	{ "astn"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songtracknumber" },
	{ "asul"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songdataurl" },
	{ "asur"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songuserrating" },
	{ "asvc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songprimaryvideocodec" },
	{ "asyr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.songyear" },
	{ "ated"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"daap.supportsextradata" },
	{ "avdb"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"daap.serverdatabases" },
	{ "awrk"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"daap.songwork" },
	{ "caar"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.availablerepeatstates" },
	{ "caas"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.availableshufflestates" },
	{ "caci"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"caci" },
	{ "cafe"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.fullscreenenabled" },
	{ "cafs"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.fullscreen" },
	{ "caia"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.isactive" },
	{ "cana"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dacp.nowplayingartist" },
	{ "cang"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dacp.nowplayinggenre" },
	{ "canl"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dacp.nowplayingalbum" },
	{ "cann"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dacp.nowplayingname" },
	{ "canp"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.nowplayingids" },
	{ "cant"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.nowplayingtime" },
	{ "capr"_u32,	DMAP_VERS, DMAP_UNKNOWN,	"dacp.protocolversion" },
	{ "caps"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.playerstate" },
	{ "carp"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.repeatstate" },
	{ "cash"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.shufflestate" },
	{ "casp"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dacp.speakers" },
	{ "cast"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.songtime" },
	{ "cavc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.volumecontrollable" },
	{ "cave"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.visualizerenabled" },
	{ "cavs"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dacp.visualizer" },
	{ "ceJC"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.jukebox-client-vote" },
	{ "ceJI"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.jukebox-current" },
	{ "ceJS"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.jukebox-score" },
	{ "ceJV"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.jukebox-vote" },
	{ "ceQR"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"com.apple.itunes.playqueue-contents-response" },
	{ "ceQa"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.playqueue-album" },
	{ "ceQg"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.playqueue-genre" },
	{ "ceQn"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.playqueue-name" },
	{ "ceQr"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"com.apple.itunes.playqueue-artist" },
	{ "cmgt"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmcp.getpropertyresponse" },
	{ "cmmk"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmcp.mediakind" },
	{ "cmpr"_u32,	DMAP_VERS, DMAP_UNKNOWN,	"dmcp.protocolversion" },
	{ "cmsr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmcp.serverrevision" },
	{ "cmst"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmcp.playstatus" },
	{ "cmvo"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmcp.volume" },
	{ "f\215ch"_u32,DMAP_UINT, DMAP_UNKNOWN,	"dmap.haschildcontainers" },
	{ "ipsa"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dpap.iphotoslideshowadvancedoptions" },
	{ "ipsl"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dpap.iphotoslideshowoptions" },
	{ "mbcl"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmap.bag" },
	{ "mccr"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmap.contentcodesresponse" },
	{ "mcna"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dmap.contentcodesname" },
	{ "mcnm"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.contentcodesnumber" },
	{ "mcon"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmap.container" },
	{ "mctc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.containercount" },
	{ "mcti"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.containeritemid" },
	{ "mcty"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.contentcodestype" },
	{ "mdbk"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.databasekind" },
	{ "mdcl"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmap.dictionary" },
	{ "mdst"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.downloadstatus" },
	{ "meds"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.editcommandssupported" },
	{ "mimc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.itemcount" },
	{ "minm"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dmap.itemname" },
	{ "mlcl"_u32,	DMAP_DICT, DMAP_DICT,		"dmap.listing" },
	{ "mlid"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.sessionid" },
	{ "mlit"_u32,	DMAP_ITEM, DMAP_UNKNOWN,	"dmap.listingitem" },
	{ "mlog"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmap.loginresponse" },
	{ "mpco"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.parentcontainerid" },
	{ "mper"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.persistentid" },
	{ "mpro"_u32,	DMAP_VERS, DMAP_UNKNOWN,	"dmap.protocolversion" },
	{ "mrco"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.returnedcount" },
	{ "mrpr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.remotepersistentid" },
	{ "msal"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.supportsautologout" },
	{ "msas"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.authenticationschemes" },
	{ "msau"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.authenticationmethod" },
	{ "msbr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.supportsbrowse" },
	{ "msdc"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.databasescount" },
	{ "msex"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.supportsextensions" },
	{ "msix"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.supportsindex" },
	{ "mslr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.loginrequired" },
	{ "msma"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.machineaddress" },
	{ "msml"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"msml" },
	{ "mspi"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.supportspersistentids" },
	{ "msqy"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.supportsquery" },
	{ "msrs"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.supportsresolve" },
	{ "msrv"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmap.serverinforesponse" },
	{ "mstc"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"dmap.utctime" },
	{ "mstm"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.timeoutinterval" },
	{ "msto"_u32,	DMAP_INT,  DMAP_UNKNOWN,	"dmap.utcoffset" },
	{ "msts"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dmap.statusstring" },
	{ "mstt"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.status" },
	{ "msup"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.supportsupdate" },
	{ "mtco"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.specifiedtotalcount" },
	{ "mudl"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmap.deletedidlisting" },
	{ "mupd"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dmap.updateresponse" },
	{ "musr"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.serverrevision" },
	{ "muty"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dmap.updatetype" },
	{ "pasp"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dpap.aspectratio" },
	{ "pcmt"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dpap.imagecomments" },
	{ "peak"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.photos.album-kind" },
	{ "peed"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"com.apple.itunes.photos.exposure-date" },
	{ "pefc"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"com.apple.itunes.photos.faces" },
	{ "peki"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"com.apple.itunes.photos.key-image-id" },
	{ "pekm"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"com.apple.itunes.photos.key-image" },
	{ "pemd"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"com.apple.itunes.photos.modification-date" },
	{ "pfai"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dpap.failureids" },
	{ "pfdt"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dpap.filedata" },
	{ "pfmt"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dpap.imageformat" },
	{ "phgt"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dpap.imagepixelheight" },
	{ "picd"_u32,	DMAP_DATE, DMAP_UNKNOWN,	"dpap.creationdate" },
	{ "pifs"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dpap.imagefilesize" },
	{ "pimf"_u32,	DMAP_STR,  DMAP_UNKNOWN,	"dpap.imagefilename" },
	{ "plsz"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dpap.imagelargefilesize" },
	{ "ppro"_u32,	DMAP_VERS, DMAP_UNKNOWN,	"dpap.protocolversion" },
	{ "prat"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dpap.imagerating" },
	{ "pret"_u32,	DMAP_DICT, DMAP_UNKNOWN,	"dpap.retryids" },
	{ "pwth"_u32,	DMAP_UINT, DMAP_UNKNOWN,	"dpap.imagepixelwidth" }
};

struct dmap_parser {
	static const dmap_field *field_from_code(uint32 code) {
		auto	i = lower_boundc(dmap_fields, code, [](const dmap_field *a, uint32 code) { return a->code < code;} );
		return i != end(dmap_fields) && i->code == code ? i : 0;
	}

	static const char *name_from_code(uint32 code) {
		auto field = field_from_code(code);
		return field ? field->name : 0;
	}

	bool parse(const char *buf, size_t len, const dmap_field *parent = 0);

	void on_dict_start(uint32 code, const char *name) override;
	void on_dict_end(uint32 code, const char *name) override;

	void on_int		(uint32 code, const char *name, int64 value) override;
	void on_uint	(uint32 code, const char *name, uint64 value) override;
	void on_string	(uint32 code, const char *name, const char *buf, size_t len) override;
	void on_data	(uint32 code, const char *name, const char *buf, size_t len) override;
	void on_date	(uint32 code, const char *name, uint32 value) override;
};

bool dmap_parser::parse(const char *buf, size_t len, const dmap_field *parent) {
	if (!buf)
		return false;

	const char *p	= buf, *end = buf + len;
	while (end - p >= 8) {
		uint32	code		= ((uint32be*)p)[0];
		uint32	field_len	= ((uint32be*)p)[1];
		p += 8;

		if (p + field_len > end)
			return -1;

		DMAP_TYPE	field_type	= DMAP_UNKNOWN;
		const char *field_name	= 0;
		auto*		field		= field_from_code(code);

		if (field) {
			field_type = field->type;
			field_name = field->name;

			if (field_type == DMAP_ITEM)
				field_type = parent && parent->list_item_type ? parent->list_item_type :  DMAP_DICT;

		} else {
			// Make a best guess of the type
			// Look for a four char code followed by a length within the current field
			if (field_len >= 8 && is_alpha(p[0]) && is_alpha(p[1]) && is_alpha(p[2]) && is_alpha(p[3]) && *(uint32be*)(p + 4) < field_len) {
				field_type = DMAP_DICT;

			} else {
				bool is_string = true;
				for (int i = 0; is_string && i < field_len; i++)
					is_string = is_print(p[i]);

				field_type = is_string ? DMAP_STR : field_len <= 8 && is_pow2(field_len) ? DMAP_UINT : DMAP_DATA;
			}
		}

		switch (field_type) {
			case DMAP_UINT:
				on_uint(code, field_name, read_bytes<uint64be>(p, field_len));
				break;
			case DMAP_INT:
				on_int(code, field_name, read_bytes<uint64be>(p, field_len));
				break;
			case DMAP_STR:
				on_string(code, field_name, p, field_len);
				break;
			case DMAP_DATA:
				on_data(code, field_name, p, field_len);
				break;
			case DMAP_DATE:
				on_date(code, field_name, *(uint32be*)p);
				break;
			case DMAP_VERS:
				if (field_len >= 4)
					on_uint(code, field_name, *(uint32be*)p);
				break;
			case DMAP_DICT:
				on_dict_start(code, field_name);
				if (parse(p, field_len, field) != 0)
					return -1;
				on_dict_end(code, field_name);
				break;

			case DMAP_ITEM:	// Unreachable: listing item types are always mapped to another type
			case DMAP_UNKNOWN:
				break;
		}

		p += field_len;
	}

	return p == end;
}

}
#endif
