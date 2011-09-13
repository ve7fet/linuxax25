#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/* MS-DOS time/date conversion routines derived from: */

/*
 *  linux/fs/msdos/misc.c
 *
 *  Written 1992,1993 by Werner Almesberger
 */

/* Linear day numbers of the respective 1sts in non-leap years. */

static int day_n[] =
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 0, 0, 0, 0 };
		  /* JanFebMarApr May Jun Jul Aug Sep Oct Nov Dec */


/* Convert a MS-DOS time/date pair to a UNIX date (seconds since 1 1 70). */

int date_dos2unix(unsigned short time, unsigned short date)
{
	int month, year, secs;

	month = ((date >> 5) & 15) - 1;
	year = date >> 9;
	secs =
	    (time & 31) * 2 + 60 * ((time >> 5) & 63) +
	    (time >> 11) * 3600 + 86400 * ((date & 31) - 1 + day_n[month] +
					   (year / 4) + year * 365 -
					   ((year & 3) == 0
					    && month < 2 ? 1 : 0) + 3653);
	/* days since 1.1.70 plus 80's leap day */
	return secs;
}


/* Convert linear UNIX date to a MS-DOS time/date pair. */

void date_unix2dos(time_t unix_date, unsigned short *time,
		   unsigned short *date)
{
	int day, year, nl_day, month;

	*time = (unix_date % 60) / 2 + (((unix_date / 60) % 60) << 5) +
	    (((unix_date / 3600) % 24) << 11);
	day = unix_date / 86400 - 3652;
	year = day / 365;
	if ((year + 3) / 4 + 365 * year > day)
		year--;
	day -= (year + 3) / 4 + 365 * year;
	if (day == 59 && !(year & 3)) {
		nl_day = day;
		month = 2;
	} else {
		nl_day = (year & 3) || day <= 59 ? day : day - 1;
		for (month = 0; month < 12; month++)
			if (day_n[month] > nl_day)
				break;
	}
	*date = nl_day - day_n[month - 1] + 1 + (month << 5) + (year << 9);
}

/* Convert yapp format 8 hex characters into Unix time */

int yapp2unix(char *ytime)
{
	int i;
	unsigned short time, date;
	if (strlen(ytime) != 8)
		return 0;
	for (i = 0; i < 8; i++)
		if (!isxdigit(ytime[i]))
			return 0;
	time = strtoul(ytime + 4, (char **) NULL, 16);
	ytime[4] = 0;
	date = strtoul(ytime, (char **) NULL, 16);
	return (date_dos2unix(time, date));

}

/* Convert unix time to 8 character yapp hex format */

void unix2yapp(time_t unix_date, char *buffer)
{
	unsigned short time, date;
	date_unix2dos(unix_date, &time, &date);
	sprintf(buffer, "%04X%04X", date, time);
}
