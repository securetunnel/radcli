/*
 * Copyright (C) 1995,1996,1997 Lars Fenneberg
 *
 * Copyright 1992 Livingston Enterprises, Inc.
 *
 * Copyright 1992,1993, 1994,1995 The Regents of the University of Michigan
 * and Merit Network, Inc. All Rights Reserved
 *
 * See the file COPYRIGHT for the respective terms and conditions.
 * If the file is missing contact me at lf@elemental.net
 * and I'll send you a copy.
 *
 */

/**
 * @defgroup radcli-api Main API
 * @brief Main API Functions
 *
 * @{
 */

#include <config.h>
#include <includes.h>
#include <radcli/radcli.h>
#include "util.h"

/** Add attribute to dictionary
 *
 * Does not check if such attribute already exists
 *
 * @param rh              a handle to configuration.
 * @param namestr         attribute name
 * @param type            attribute type
 * @param value           attribute value
 * @param vendorspec      vendorspec
 * @return                added attr on success, NULL on failure
 */
DICT_ATTR *rc_dict_addattr(rc_handle *rh, char const * namestr, uint32_t value, int type, uint32_t vendorspec)
{
	DICT_ATTR *attr;

	if (strlen (namestr) > NAME_LENGTH)
	{
		rc_log(LOG_ERR, "rc_dict_addattr: invalid attribute length");
		return NULL;
	}

	if (type < 0 || type >= PW_TYPE_MAX)
	{
		rc_log(LOG_ERR, "rc_dict_addattr: invalid attribute type");
		return NULL;
	}

	/* Create a new attribute for the list */
	if ((attr = malloc(sizeof (DICT_ATTR))) == NULL)
	{
		rc_log(LOG_CRIT, "rc_dict_addattr: out of memory");
		return NULL;
	}

	strlcpy(attr->name, namestr, sizeof(attr->name));
	attr->value = VATTRID_SET(value, vendorspec);
	attr->type = type;

	/* Insert it into the list */
	attr->next = rh->dictionary_attributes;
	rh->dictionary_attributes = attr;
	return attr;
}

/** Add value to dictionary
 *
 * Does not check if such value already exists
 *
 * @param rh              a handle to configuration.
 * @param attrstr         attribute name
 * @param namestr         name
 * @param value           attribute value
 * @return                added value on success, NULL on failure
 */
DICT_VALUE *rc_dict_addval(rc_handle *rh, char const * attrstr, char const * namestr, uint32_t value)
{
	DICT_VALUE *dval;

	if (strlen(attrstr) > NAME_LENGTH)
	{
		rc_log(LOG_ERR, "rc_dict_addval: invalid attribute length");
		return NULL;
	}

	if (strlen(namestr) > NAME_LENGTH)
	{
		rc_log(LOG_ERR, "rc_dict_addval: invalid name length");
		return NULL;
	}

	/* Create a new VALUE entry for the list */
	if ((dval = malloc(sizeof (DICT_VALUE))) == NULL)
	{
		rc_log(LOG_CRIT, "rc_dict_addval: out of memory");
		return NULL;
	}
	strlcpy(dval->attrname, attrstr, sizeof(dval->attrname));
	strlcpy(dval->name, namestr, sizeof(dval->name));
	dval->value = value;

	/* Insert it into the list */
	dval->next = rh->dictionary_values;
	rh->dictionary_values = dval;
	return dval;
}

/** Add vendor to dictionary
 *
 * Does not check if such vendor already exists
 *
 * @param rh              a handle to configuration.
 * @param namestr         vendor name
 * @param vendorspec      vendorspec
 * @return                added value on success, NULL on failure
 */
DICT_VENDOR *rc_dict_addvend(rc_handle *rh, char const * namestr, uint32_t vendorspec)
{
	DICT_VENDOR *dvend;

	if (strlen(namestr) > NAME_LENGTH)
	{
		rc_log(LOG_ERR, "rc_dict_addvend: invalid vendor name length");
		return NULL;
	}

	/* Create a new VENDOR entry for the list */
	dvend = malloc(sizeof(DICT_VENDOR));
	if (dvend == NULL)
	{
		rc_log(LOG_CRIT, "rc_dict_init: out of memory");
		return NULL;
	}
	strlcpy(dvend->vendorname, namestr, sizeof(dvend->vendorname));
	dvend->vendorpec = vendorspec;

	/* Insert it into the list */
	dvend->next = rh->dictionary_vendors;
	rh->dictionary_vendors = dvend;
	return dvend;
}

/** Parse the input dictionary-config and initialize the dictionary.
 *
 * Read all ATTRIBUTES into the dictionary_attributes list.
 * Read all VALUES into the dictionary_values list.
 *
 * @param rh       a handle to parsed configuration.
 * @param dictfd   a handle to the dictionary config.
 * @param filename the name of the dictionary file.
 * @return 0 on success, -1 on failure.
 */
static int rc_dict_init(rc_handle *rh, FILE *dictfd, char const *filename)
{
	char            dummystr[AUTH_ID_LEN];
	char            namestr[AUTH_ID_LEN];
	char            valstr[AUTH_ID_LEN];
	char            attrstr[AUTH_ID_LEN];
	char            typestr[AUTH_ID_LEN];
	char            optstr[AUTH_ID_LEN];
	char            ifilename[PATH_MAX] = {0};
	char            *cp;
	int             line_no = 0;
	DICT_ATTR      *attr;
	DICT_VALUE     *dval;
	DICT_VENDOR    *dvend;
	char            buffer[256];
	uint32_t        value;
	int             type;
	unsigned attr_vendorspec = 0;
	const char *pfilename = filename;

	if (pfilename == NULL) 
	{
		pfilename = "memory";
	}        

	while (fgets (buffer, sizeof (buffer), dictfd) != NULL)
	{
		line_no++;

		/* Skip empty space */
		if (*buffer == '#' || *buffer == '\0' || *buffer == '\n' || \
		    *buffer == '\r')
		{
			continue;
		}

		/* Strip out comments */
		cp = strchr(buffer, '#');
		if (cp != NULL)
		{
			*cp = '\0';
		}

		if (strncmp (buffer, "ATTRIBUTE", 9) == 0)
		{
			optstr[0] = '\0';
			/* Read the ATTRIBUTE line */
			if (sscanf (buffer, "%63s%63s%63s%63s%63s", dummystr, namestr,
				valstr, typestr, optstr) < 4)
			{
				rc_log(LOG_ERR, 
					"rc_dict_init: invalid attribute on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}

			/*
			 * Validate all entries
			 */
			if (strlen (namestr) > NAME_LENGTH)
			{
				rc_log(LOG_ERR, 
					"rc_dict_init: invalid name length on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}

			if (!isdigit (*valstr))
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid value on line %d of dictionary %s",
					line_no, pfilename);
				return -1;
			}
			value = atoi (valstr);

			if (strcmp (typestr, "string") == 0)
			{
				type = PW_TYPE_STRING;
			}
			else if (strcmp (typestr, "integer") == 0)
			{
				type = PW_TYPE_INTEGER;
			}
			else if (strcmp (typestr, "ipaddr") == 0)
			{
				type = PW_TYPE_IPADDR;
			}
			else if (strcmp (typestr, "ipv4addr") == 0)
			{
				type = PW_TYPE_IPADDR;
			}
			else if (strcmp (typestr, "ipv6addr") == 0)
			{
				type = PW_TYPE_IPV6ADDR;
			}
			else if (strcmp (typestr, "ipv6prefix") == 0)
			{
				type = PW_TYPE_IPV6PREFIX;
			}
			else if (strcmp (typestr, "date") == 0)
			{
				type = PW_TYPE_DATE;
			}
			else
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid type on line %d of dictionary %s",
					line_no, pfilename);
				return -1;
			}

			dvend = NULL;
			if (optstr[0] != '\0') {
				char *cp1;
				for (cp1 = optstr; cp1 != NULL; cp1 = cp) {
					cp = strchr(cp1, ',');
					if (cp != NULL) {
						*cp = '\0';
						cp++;
					}
					if (strncmp(cp1, "vendor=", 7) == 0)
						cp1 += 7;
					dvend = rc_dict_findvend(rh, cp1);
					if (dvend == NULL) {
						rc_log(LOG_ERR,
							"rc_dict_init: unknown Vendor-Id %s on line %d of "
							"dictionary %s", cp1, line_no, pfilename);
						return -1;
					}
				}
			}

			/* Create a new attribute for the list */
			if ((attr = malloc (sizeof (DICT_ATTR))) == NULL)
			{
				rc_log(LOG_CRIT, "rc_dict_init: out of memory");
				return -1;
			}
			strcpy (attr->name, namestr);
			attr->type = type;

			if (dvend != NULL) {
				attr->value = VATTRID_SET(value, dvend->vendorpec);
			} else {
				attr->value = VATTRID_SET(value, attr_vendorspec);
			}

			/* Insert it into the list */
			attr->next = rh->dictionary_attributes;
			rh->dictionary_attributes = attr;
		}
		else if (strncmp (buffer, "VALUE", 5) == 0)
		{
			/* Read the VALUE line */
			if (sscanf (buffer, "%63s%63s%63s%63s", dummystr, attrstr,
				    namestr, valstr) != 4)
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid value entry on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}

			/*
			 * Validate all entries
			 */
			if (strlen (attrstr) > NAME_LENGTH)
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid attribute length on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}

			if (strlen (namestr) > NAME_LENGTH)
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid name length on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}

			if (!isdigit (*valstr))
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid value on line %d of dictionary %s",
					line_no, pfilename);
				return -1;
			}
			value = atoi (valstr);

			/* Create a new VALUE entry for the list */
			if ((dval = malloc (sizeof (DICT_VALUE))) == NULL)
			{
				rc_log(LOG_CRIT, "rc_dict_init: out of memory");
				return -1;
			}
			strcpy (dval->attrname, attrstr);
			strcpy (dval->name, namestr);
			dval->value = value;

			/* Insert it into the list */
			dval->next = rh->dictionary_values;
			rh->dictionary_values = dval;
		}
		else if ((filename != NULL) && 
				(strncmp (buffer, "$INCLUDE", 8) == 0))
		{
			/* Read the $INCLUDE line */
			if (sscanf (buffer, "%63s%63s", dummystr, namestr) != 2)
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid include entry on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}
			strncpy(ifilename, namestr, sizeof(ifilename));
			/* Append directory if necessary */
			if (namestr[0] != '/') {
				cp = strrchr(filename, '/');
				if (cp != NULL) {
					*cp = '\0';
					snprintf(ifilename, sizeof(ifilename), "%s/%s", filename, namestr);
					*cp = '/';
				}
			}
			if (rc_read_dictionary(rh, ifilename) < 0)
			{
				return -1;
			}
		}
		else if (strncmp (buffer, "END-VENDOR", 10) == 0)
		{
			attr_vendorspec = 0;
		}
		else if (strncmp (buffer, "BEGIN-VENDOR", 12) == 0)
		{
			DICT_VENDOR *v;
			/* Read the vendor name */
			if (sscanf (buffer+12, "%63s", dummystr) != 1)
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid Vendor-Id on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}

			v = rc_dict_findvend(rh, dummystr);
			if (v == NULL) {
				rc_log(LOG_ERR,
					"rc_dict_init: unknown Vendor %s on line %d of "
					"dictionary %s", dummystr, line_no, pfilename);
				return -1;
			}

			attr_vendorspec = v->vendorpec;
		}
		else if (strncmp (buffer, "VENDOR", 6) == 0)
		{
			/* Read the VALUE line */
			if (sscanf (buffer, "%63s%63s%63s", dummystr, attrstr, valstr) != 3)
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid Vendor-Id on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}

			/* Validate all entries */
			if (strlen (attrstr) > NAME_LENGTH)
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid attribute length on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}

			if (!isdigit (*valstr))
			{
				rc_log(LOG_ERR,
					"rc_dict_init: invalid Vendor-Id on line %d of "
					"dictionary %s", line_no, pfilename);
				return -1;
			}
			value = atoi (valstr);

			/* Create a new VENDOR entry for the list */
			dvend = malloc(sizeof(DICT_VENDOR));
			if (dvend == NULL)
			{
				rc_log(LOG_CRIT, "rc_dict_init: out of memory");
				return -1;
			}
			strcpy (dvend->vendorname, attrstr);
			dvend->vendorpec = value;

			/* Insert it into the list */
			dvend->next = rh->dictionary_vendors;
			rh->dictionary_vendors = dvend;
		}
	}
	return 0;
}

/** Initialize the dictionary
 *
 * Read all ATTRIBUTES into the dictionary_attributes list.
 * Read all VALUES into the dictionary_values list.
 *
 * @param rh a handle to parsed configuration.
 * @param filename the name of the dictionary file.
 * @return 0 on success, -1 on failure.
 */
int rc_read_dictionary (rc_handle *rh, char const *filename)
{
	FILE    *dictfd;
	int     ret_val = 0;

	if (rh->first_dict_read != NULL && strcmp(filename, rh->first_dict_read) == 0)
		return 0;

	if ((dictfd = fopen (filename, "r")) == NULL)
	{
		rc_log(LOG_ERR, "rc_read_dictionary couldn't open dictionary %s: %s",
				filename, strerror(errno));
		return -1;
	}

	ret_val = rc_dict_init(rh, dictfd, filename);

	fclose (dictfd);

	if (rh->first_dict_read == NULL)
		rh->first_dict_read = strdup(filename);

	return ret_val;
}

/** Initialize the dictionary from Buffer
 *
 * Read all ATTRIBUTES into the dictionary_attributes list.
 * Read all VALUES into the dictionary_values list.
 *
 * @param rh   a handle to parsed configuration.
 * @param buf  buffer holding Dictionary info
 * @param size size of buffer
 * @return 0 on success, -1 on failure.
 */
int rc_read_dictionary_from_buffer (rc_handle *rh, char const *buf, size_t size)
{
	FILE      *dictfd;
	int       ret_val = 0;

	if ((dictfd = fmemopen ((void *)buf, size, "r")) == NULL)
	{
		rc_log(LOG_ERR, "rc_read_dictionary_from_buffer failed to read "
				"input buffer %s", strerror(errno));
		return -1;
	}

	ret_val = rc_dict_init(rh, dictfd, NULL);

	fclose (dictfd);

	return ret_val;
}

/** Lookup a DICT_ATTR by attribute number
 *
 * @param rh a handle to parsed configuration.
 * @param attribute the attribute ID.
 * @return the full attribute structure based on the attribute id number.
 */
DICT_ATTR *rc_dict_getattr(rc_handle const *rh, uint64_t attribute)
{
	DICT_ATTR      *attr;

	attr = rh->dictionary_attributes;
	while (attr != NULL)
	{
		if (attr->value == attribute)
		{
			return attr;
		}
		attr = attr->next;
	}
	return NULL;
}

/** Lookup a DICT_ATTR by its name
 *
 * @param rh a handle to parsed configuration.
 * @param attrname the attribute name.
 *
 * @return the full attribute structure based on the attribute name.
 */
DICT_ATTR *rc_dict_findattr(rc_handle const *rh, char const *attrname)
{
	DICT_ATTR      *attr;

	attr = rh->dictionary_attributes;
	while (attr != NULL)
	{
		if (strcasecmp (attr->name, attrname) == 0)
		{
			return attr;
		}
		attr = attr->next;
	}
	return NULL;
}


/** Lookup a DICT_VALUE by its name
 *
 * @param rh a handle to parsed configuration.
 * @param valname the value name.
 * @return the full value structure based on the value name.
 */
DICT_VALUE *rc_dict_findval(rc_handle const *rh, char const *valname)
{
	DICT_VALUE     *val;
	val = rh->dictionary_values;
	while (val != NULL)
	{
		if (strcasecmp (val->name, valname) == 0)
		{
			return val;
		}
		val = val->next;
	}
	return NULL;
}

/** Lookup a DICT_VENDOR by its name
 *
 * @param rh a handle to parsed configuration.
 * @param vendorname the vendor name.
 * @return the full vendor structure based on the vendor name.
 */
DICT_VENDOR *rc_dict_findvend(rc_handle const *rh, char const *vendorname)
{
	DICT_VENDOR	*vend;

	for (vend = rh->dictionary_vendors; vend != NULL; vend = vend->next)
		if (strcasecmp(vend->vendorname, vendorname) == 0)
			return vend;
	return NULL;
}

/** Lookup a DICT_VENDOR by its IANA number
 *
 * @param rh a handle to parsed configuration.
 * @param vendorspec the vendor ID.
 * @return the full vendor structure based on the vendor id number.
 */
DICT_VENDOR *rc_dict_getvend (rc_handle const *rh, uint32_t vendorspec)
{
        DICT_VENDOR      *vend;

	for (vend = rh->dictionary_vendors; vend != NULL; vend = vend->next)
		if (vend->vendorpec == vendorspec)
			return vend;
	return NULL;
}

/** Get DICT_VALUE based on attribute name and integer value number
 *
 * @param rh a handle to parsed configuration.
 * @param value the attribute value.
 * @param attrname the attribute name.
 * @return the full value structure based on the actual value and the associated attribute name.
 */
DICT_VALUE *rc_dict_getval(rc_handle const *rh, uint32_t value, char const *attrname)
{
	DICT_VALUE     *val;

	val = rh->dictionary_values;
	while (val != NULL)
	{
		if (strcmp (val->attrname, attrname) == 0 &&
				val->value == value)
		{
			return val;
		}
		val = val->next;
	}
	return NULL;
}

/** Frees the allocated dictionary
 *
 * @param rh a handle to parsed configuration.
 */
void rc_dict_free(rc_handle *rh)
{
	DICT_ATTR	*attr, *nattr;
	DICT_VALUE	*val, *nval;
	DICT_VENDOR	*vend, *nvend;

	for (attr = rh->dictionary_attributes; attr != NULL; attr = nattr) {
		nattr = attr->next;
		free(attr);
	}
	for (val = rh->dictionary_values; val != NULL; val = nval) {
		nval = val->next;
		free(val);
	}
	for (vend = rh->dictionary_vendors; vend != NULL; vend = nvend) {
		nvend = vend->next;
		free(vend);
	}
	rh->dictionary_attributes = NULL;
	rh->dictionary_values = NULL;
	rh->dictionary_vendors = NULL;
}
/** @} */
