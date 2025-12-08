#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
  char *timezonestring;
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
                api.timezonestring, api.school, api.shafaq);

  if (n < 0)
    return NULL;

  size = (size_t)n + 1;
  fmt = malloc (size);
  if (fmt == NULL)
    return NULL;

  n = snprintf (fmt, size, ADAN_API_URL_FMT, date, api.address, api.method,
                api.timezonestring, api.school, api.shafaq);
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

int
main (void)
{
  CURL *curl = NULL;
  CURLU *urlp;
  CURLUcode uc;
  adan_response_t adan_data = { 0 };

  CURLcode res = curl_global_init (CURL_GLOBAL_ALL);
  if (res)
    {
      return (int)res;
    }

  curl = curl_easy_init ();

  if (curl == NULL)
    {
      fprintf (stderr, "error: failed to initialize http client\n");
      curl_global_cleanup ();
      return (int)res;
    }

  adan_api_t adan_api = {
    .date = time (NULL),
    .address = "Rabat, Morocco, MA",
    .timezonestring = "Africa/Casablanca",
    .method = 21,
    .shafaq = "general",
    .school = 1,
  };
  char *url = adan_api_url_create (adan_api);

  urlp = curl_url ();
  uc = curl_url_set (urlp, CURLUPART_URL, url,
                     CURLU_URLENCODE | CURLU_ALLOW_SPACE);

  if (uc)
    {
      fprintf (stderr, "error: failed to set url: %s\n",
               curl_easy_strerror ((CURLcode)uc));
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
      free (adan_data.response);
      free (url);
      curl_url_cleanup (urlp);
      curl_easy_cleanup (curl);
      curl_global_cleanup ();
      return (int)res;
    }

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
