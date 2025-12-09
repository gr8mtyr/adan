#include <assert.h>
#include <error.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <argp.h>
#include <curl/curl.h>
#include <jansson.h>

// Refer to
// `https://aladhan.com/prayer-times-api#get-/nextPrayerByAddress/-date-`
// for how to fill this api url
#define ADAN_API_URL_FMT                                                      \
  "https://api.aladhan.com/v1/nextPrayerByAddress/%s?"                        \
  "address=%s"                                                                \
  "&method=%d"                                                                \
  "&timezonestring=%s"                                                        \
  "&school=%d"                                                                \
  "&shafaq=%s"

typedef struct
{
  time_t date;
  char *address;
  int method;
  char *timezone;
  int school;
  char *shafaq;
} adan_api_t;

static char *
adan_api_url_create (adan_api_t api)
{
  int n = 0;
  size_t size = 0;
  char *fmt = NULL;

  char date[11];
  strftime (date, 11, "%d-%m-%Y", localtime (&api.date));

  n = snprintf (fmt, size, ADAN_API_URL_FMT, date, api.address, api.method,
                api.timezone, api.school, api.shafaq);

  if (n < 0)
    return NULL;

  size = (size_t)n + 1;
  fmt = malloc (size);
  if (fmt == NULL)
    return NULL;

  n = snprintf (fmt, size, ADAN_API_URL_FMT, date, api.address, api.method,
                api.timezone, api.school, api.shafaq);
  if (n < 0)
    {
      free (fmt);
      return NULL;
    }

  return fmt;
}

typedef struct
{
  char *response;
  size_t size;
} adan_response_t;

static size_t
adan_curl_write_cb (const char *ptr, size_t size, size_t nmemb, void *userdata)
{
  assert (size == 1);

  size_t realsize = nmemb;
  adan_response_t *adan = (adan_response_t *)userdata;

  char *data = realloc (adan->response, adan->size + realsize + 1);
  if (!data)
    return 0;

  adan->response = data;
  memcpy (&(adan->response[adan->size]), ptr, realsize);
  adan->size += realsize;
  adan->response[adan->size] = 0;

  return realsize;
}

const char *argp_program_version = "adan 0.0.0";
const char *argp_program_bug_address = "<mahmoudessehayli@gmail.com>";
const char cli_doc[] = "Adan -- find the next prayer time";

// --address
#define CLI_OPT_ADDRESS (0x80 + 1)
// --timezone
#define CLI_OPT_TIMEZONE (0x80 + 2)
// --method
#define CLI_OPT_METHOD (0x80 + 3)
// --shafaq
#define CLI_OPT_SHAFAQ (0x80 + 4)
// --school
#define CLI_OPT_SCHOOL (0x80 + 5)

struct argp_option cli_argp_options[] = {
  {
      "address",
      CLI_OPT_ADDRESS,
      "ADDRESS",
      0,
      "Address of the user location",
      0,
  },
  {
      "timezone",
      CLI_OPT_TIMEZONE,
      "TIMEZONE",
      0,
      "Valid timzone name\n"
      "see 'https://php.net/manual/en/timezones.php'",
      0,
  },
  {
      "method",
      CLI_OPT_METHOD,
      "METHOD",
      0,
      "Prayer times calcuation method\n"
      "possible value: [0-23]\n"
      "see 'https://aladhan.com/calculation-methods'",
      0,
  },
  {
      "shafaq",
      CLI_OPT_SHAFAQ,
      "SHAFAQ",
      0,
      "Which Shafaq to use if the method is 'Moonsighting Commitee "
      "Worldwide'\n"
      "possible values: ['general', 'ahmer', 'abyad']",
      0,
  },
  {
      "school",
      CLI_OPT_SCHOOL,
      "SCHOOL",
      0,
      "Shafi(0) or Hanafi(1)",
      0,
  },
  { 0 },
};

typedef struct
{
  char *address;  // --address
  char *timezone; // --timezone
  int method;     // --method
  char *shafaq;   // --shafaq
  int school;     // --school
} cli_args_t;

static error_t
cli_argp_parser (int key, char *arg, struct argp_state *state)
{
  cli_args_t *cli_args = state->input;

  switch (key)
    {
    case CLI_OPT_ADDRESS:
      cli_args->address = arg;
      break;
    case CLI_OPT_TIMEZONE:
      cli_args->timezone = arg;
      break;
    case CLI_OPT_METHOD:
      cli_args->method = arg ? atoi (arg) : 3;
      break;
    case CLI_OPT_SHAFAQ:
      cli_args->shafaq = arg;
      break;
    case CLI_OPT_SCHOOL:
      cli_args->school = arg ? atoi (arg) : 0;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return EXIT_SUCCESS;
}

struct argp cli_argp = {
  .doc = cli_doc,
  .options = cli_argp_options,
  .parser = cli_argp_parser,
  0,
};

#define ETC_TIMEZONE "/etc/timezone"
#define ETC_LOCALTIME "/etc/localtime"

char *
get_etc_timezone ()
{
  // /etc/timezone file stat
  struct stat et_fs = { 0 };
  // check if file exist if not we go to the next the check
  int fs_err = stat (ETC_TIMEZONE, &et_fs);
  if (fs_err == -1)
    return NULL;

  // check if the timezone file is a regular file, apparently in some systems
  // this path is a directory
  if (!S_ISREG (et_fs.st_mode))
    return NULL;

  // open the /etc/timezone as its a file that contains the timezone so we
  // need to read the its content to find it, we check for null to be sure
  FILE *et_fp = fopen (ETC_TIMEZONE, "r+");
  if (et_fp == NULL)
    return NULL;

  // the file size should be just the size of the timezone string so we
  // can use it as the size of the string we return
  fseek (et_fp, 0, SEEK_END);
  int et_fp_size = fseek (et_fp, 0, SEEK_SET);
  rewind (et_fp);

  // we allocate the string on the heap and fill it with the content from the
  // file and return it
  char *timezone_s = NULL;
  size_t timezone_s_size = sizeof (char) * et_fp_size;
  timezone_s = malloc (timezone_s_size + 1);
  if (timezone_s == NULL)
    return NULL;
  memset (timezone_s, 0, timezone_s_size);

  size_t read_bytes
      = fread (timezone_s, sizeof (char), timezone_s_size, et_fp);
  if ((int)read_bytes < et_fp_size)
    return NULL;

  fclose (et_fp);

  return timezone_s;
}

char *
get_etc_localtime ()
{
  // /etc/localtime file stat
  struct stat el_fs = { 0 };
  int fs_err = lstat (ETC_LOCALTIME, &el_fs);
  if (fs_err == -1)
    return NULL;

  if (!S_ISLNK (el_fs.st_mode))
    return NULL;

  char *localtime_path = realpath (ETC_LOCALTIME, NULL);
  if (localtime_path == NULL)
    return NULL;

  char *path_iter = strtok (localtime_path, "/");
  char *path_names[PATH_MAX];
  size_t path_len = 0;

  while (path_iter != NULL)
    {
      path_names[path_len] = path_iter;
      path_iter = strtok (NULL, "/");
      path_len++;
    }

  // size of the path name before last
  int path_len_0 = strlen (path_names[path_len - 2]);
  // size of the last path name
  int path_len_1 = strlen (path_names[path_len - 1]);

  char *localtime_s = NULL;
  size_t localtime_s_len = sizeof (char) * (path_len_0 + path_len_1) + 1;
  localtime_s = malloc (localtime_s_len + 1);
  if (localtime_s == NULL)
    return NULL;
  memset (localtime_s, 0, localtime_s_len);

  strncat (localtime_s, path_names[path_len - 2], path_len_0);
  strncat (localtime_s, "/", strlen ("/") + 1);
  strncat (localtime_s, path_names[path_len - 1], path_len_1);

  free (localtime_path);

  return localtime_s;
}

char *
get_env_tz ()
{
  char *timezone_s = getenv ("TZ");
  if (timezone_s == NULL)
    return NULL;

  return timezone_s;
}

// default to the string 'UTC' if we exhausted all possible checks
#define OPT_TZ_DEFAULT_UTC (1 << 0)

#define TZ_DEFAULT_UTC "UTC"

// most errors for this function come from I/O operations, so if there is
// no error in the 'errno' variable and the result is null that means we
// exhausted all possible choices to get the timezone string from the user
// system and we could not find it so we either fail or use a default such as
// 'UTC', as specified by the flag 'OPT_TZ_DEFAULT_UTC', the resolved timezone
// string should be freed as it guaranteed to be allocated in the heap.
char *
get_timezone (unsigned int flags)
{
  char *tz_s = get_env_tz ();
  if (tz_s != NULL)
    return tz_s;

  char *timezone_s = get_etc_timezone ();
  if (timezone_s != NULL)
    return timezone_s;

  char *localtime_s = get_etc_localtime ();
  if (localtime_s != NULL)
    return localtime_s;

  if (flags == 0)
    return NULL;

  if (flags & OPT_TZ_DEFAULT_UTC)
    return strndup (TZ_DEFAULT_UTC, strlen (TZ_DEFAULT_UTC));

  return NULL;
}

unsigned int
json_debug (const json_t *root)
{
  char *json_s = NULL;
  if (root == NULL)
    return 1;

  json_s = json_dumps (root, (size_t)JSON_INDENT (4));
  if (json_s == NULL)
    return 2;

  fprintf (stderr, "%s\n", json_s);

  free (json_s);

  return 0;
}

int
main (int argc, char **argv)
{
  // TODO: get default values from cli, environment, system, config file, in
  // that order
  char *timezone_s = get_timezone (OPT_TZ_DEFAULT_UTC);
  if (timezone_s == NULL)
    {
      size_t timezone_s_len = sizeof (char) * 3;
      timezone_s = malloc (timezone_s_len + 1);
      if (timezone_s == NULL)
        {
          fprintf (stderr, "error: could not allocate memory: %s\n",
                   strerror (errno));
          return EXIT_FAILURE;
        }
      memset (timezone_s, 0, timezone_s_len);
      strncpy (timezone_s, "UTC", timezone_s_len);
    }

  cli_args_t cli_args = {
    .address = "Trafalgar Square, London, UK",
    .timezone = timezone_s,
    .method = 3,
    .shafaq = "general",
    .school = 0,
  };

  argp_parse (&cli_argp, argc, argv, 0, 0, &cli_args);

  CURL *curl = NULL;
  CURLU *urlp;
  CURLUcode uc;
  adan_response_t adan_data = { 0 };

  CURLcode res = curl_global_init (CURL_GLOBAL_ALL);
  if (res)
    {
      free (timezone_s);
      return (int)res;
    }

  curl = curl_easy_init ();

  if (curl == NULL)
    {
      fprintf (stderr, "error: failed to initialize http client\n");
      free (timezone_s);
      curl_global_cleanup ();
      return (int)res;
    }

  adan_api_t adan_api = {
    .date = time (NULL),
    .address = cli_args.address,
    .timezone = cli_args.timezone,
    .method = cli_args.method,
    .shafaq = cli_args.shafaq,
    .school = cli_args.school,
  };
  char *url = adan_api_url_create (adan_api);

  urlp = curl_url ();
  uc = curl_url_set (urlp, CURLUPART_URL, url,
                     CURLU_URLENCODE | CURLU_ALLOW_SPACE);

  if (uc)
    {
      fprintf (stderr, "error: failed to set url: %s\n",
               curl_easy_strerror ((CURLcode)uc));
      free (timezone_s);
      free (adan_data.response);
      free (url);
      curl_url_cleanup (urlp);
      curl_easy_cleanup (curl);
      curl_global_cleanup ();
      return EXIT_FAILURE;
    }

  curl_easy_setopt (curl, CURLOPT_CURLU, urlp);
  curl_easy_setopt (curl, CURLOPT_PROTOCOLS_STR, "https");
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *)&adan_data);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, adan_curl_write_cb);

  res = curl_easy_perform (curl);

  if (res != CURLE_OK)
    {
      fprintf (stderr, "error: curl failed %s\n", curl_easy_strerror (res));
      free (timezone_s);
      free (adan_data.response);
      free (url);
      curl_url_cleanup (urlp);
      curl_easy_cleanup (curl);
      curl_global_cleanup ();
      return (int)res;
    }

  free (timezone_s);
  free (url);
  curl_url_cleanup (urlp);
  curl_easy_cleanup (curl);
  curl_global_cleanup ();

  json_t *js_root;
  json_error_t js_error;

  js_root = json_loads (adan_data.response, 0, &js_error);
  free (adan_data.response);

  if (js_root == NULL)
    {
      fprintf (stderr, "error: failed to parse response at (%d:%d): %s\n",
               js_error.line, js_error.column, js_error.text);
      return EXIT_FAILURE;
    }

  if (!json_is_object (js_root))
    {
      fprintf (stderr, "error: root is not a object\n");
      json_decref (js_root);
      return EXIT_FAILURE;
    }

  json_t *js_data = json_object_get (js_root, "data");
  if (!json_is_object (js_data))
    {
      fprintf (stderr, "error: data is not a object\n");
      json_decref (js_root);
      return EXIT_FAILURE;
    }

  json_t *js_timings = json_object_get (js_data, "timings");
  if (!json_is_object (js_timings))
    {
      fprintf (stderr, "error: timings is not a object\n");
      json_decref (js_root);
      return EXIT_FAILURE;
    }

  void *iter = json_object_iter (js_timings);
  const char *prayer_name = json_object_iter_key (iter);
  json_t *js_prayer_time = json_object_iter_value (iter);

  if (!json_is_string (js_prayer_time))
    {
      fprintf (stderr, "error: prayer time is not a string\n");
      json_decref (js_root);
      return EXIT_FAILURE;
    }
  const char *prayer_time = json_string_value (js_prayer_time);

  printf ("%s: %s\n", prayer_name, prayer_time);

  json_decref (js_root);

  return EXIT_SUCCESS;
}
