// adan - cli to display muslim prayer times
//
// Copyright (C) 2025 GrimTyr <mahmoudessehayli@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <curl/curl.h>
#include <curl/easy.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct curl_slist curl_slist;

size_t write_api_data (char *server_data, size_t size, size_t nmemb,
                       void *client_data);

typedef struct
{
  size_t size;
  char *data;
} resp_data_t;

int
main (void)
{
  resp_data_t resp_data = { 0 };

  curl_version_info (CURL_VERSION_HTTP2 | CURL_VERSION_HTTP3
                     | CURL_VERSION_IPV6 | CURL_VERSION_THREADSAFE);

  curl_global_init (CURL_GLOBAL_ALL);

  CURL *curl_handle = curl_easy_init ();

  if (!curl_handle)
    {
      curl_global_cleanup ();
      return EXIT_FAILURE;
    }

  curl_easy_setopt (curl_handle, CURLOPT_HTTPGET, 1);

  curl_easy_setopt (curl_handle, CURLOPT_URL,
                    "https://api.aladhan.com/v1/timingsByCity/"
                    "21-09-2025?city=Rabat&country=MA");
  curl_easy_setopt (curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

  curl_slist *headers = NULL;

  headers = curl_slist_append (headers, "Accept: application/json");

  curl_easy_setopt (curl_handle, CURLOPT_HTTPHEADER, headers);

  curl_easy_setopt (curl_handle, CURLOPT_WRITEFUNCTION, write_api_data);

  curl_easy_setopt (curl_handle, CURLOPT_WRITEDATA, (void *)&resp_data);

  char err_buf[CURL_ERROR_SIZE];

  curl_easy_setopt (curl_handle, CURLOPT_ERRORBUFFER, err_buf);
  err_buf[0] = 0;

  CURLcode resp_code = curl_easy_perform (curl_handle);

  if (resp_code != CURLE_OK)
    {
      size_t len = strlen (err_buf);
      fprintf (stderr, "\napi: (%d) ", resp_code);
      if (len)
        {
          fprintf (stderr, "%s%s", err_buf,
                   ((err_buf[len - 1] != '\n') ? "\n" : ""));
        }
      else
        {
          fprintf (stderr, "%s\n", curl_easy_strerror (resp_code));
        }
    }

  curl_slist_free_all (headers);

  curl_easy_cleanup (curl_handle);

  curl_global_cleanup ();

  json_error_t json_error;
  json_t *json_root = json_loads (resp_data.data, 0, &json_error);
  free (resp_data.data);

  if (!json_root)
    {
      fprintf (stderr, "error: on line: %d: %s\n", json_error.line,
               json_error.text);
      return EXIT_FAILURE;
    }

  json_t *prayer_times_by_city_data, *timings;

  prayer_times_by_city_data = json_object_get (json_root, "data");
  if (!json_is_object (prayer_times_by_city_data))
    {
      fprintf (stderr,
               "error: prayer times by city data is not a json object\n");
      json_decref (json_root);
      return EXIT_FAILURE;
    }

  timings = json_object_get (prayer_times_by_city_data, "timings");
  if (!json_is_object (timings))
    {
      fprintf (stderr,
               "error: prayer times by city timings is not a json object\n");
      json_decref (json_root);
      return EXIT_FAILURE;
    }

  json_t *fajr = json_object_get (timings, "Fajr");
  if (!json_is_string (fajr))
    {
      fprintf (stderr, "error: Fajr prayer value is not a string\n");
      json_decref (json_root);
      return EXIT_FAILURE;
    }

  const char *fajr_timing = json_string_value (fajr);

  printf ("%s\n", fajr_timing);

  json_decref (json_root);

  return EXIT_SUCCESS;
}

size_t
write_api_data (char *server_data, size_t size, size_t nmemb,
                void *client_data)
{

  size_t real_size = size * nmemb;
  resp_data_t *resp_data = (resp_data_t *)client_data;

  char *ptr = realloc (resp_data->data, resp_data->size + real_size + 1);
  if (!ptr)
    return 0;

  resp_data->data = ptr;
  memcpy (&(resp_data->data[resp_data->size]), server_data, real_size);
  resp_data->size += real_size;
  resp_data->data[resp_data->size] = 0;

  return real_size;
}
