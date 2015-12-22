#ifndef MODAPI_COMPATIBILITY_H
#define MODAPI_COMPATIBILITY_H

#define MODAPI_NETVERSION_TW06 ""
#define MODAPI_NETVERSION_TW07 "0.7 ad08a1129f9e4ca1"

enum
{
	MODAPI_CLIENTPROTOCOL_TW06 = 0,
	MODAPI_CLIENTPROTOCOL_TW07,
	MODAPI_CLIENTPROTOCOL_TW07MODAPI,
};

enum
{
	MODAPI_COMPATIBILITY_BROADCAST = 0,
	MODAPI_NB_COMPATIBILITY,
};

#endif
