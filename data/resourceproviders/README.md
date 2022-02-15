# How To Write Provider Configurations
Instruction written 2021 by Julius Künzel as part of Kdenlive (www.kdenlive.org)
## Overview
Provider configuration files are json files.
Here is an example for [freesound](https://freesound.org)
```json
{
    "name": "Freesound",
    "homepage": "https://freesound.org",
    "type": "sound",
    "integration": "buildin",
    "downloadOAuth2": true,
    "clientkey": "1234",
    "api": {
        "root": "https://freesound.org/apiv2",
        "oauth2": {
            "authorizationUrl": "https://freesound.org/apiv2/oauth2/authorize/",
            "accessTokenUrl": "https://freesound.org/apiv2/oauth2/access_token/",
            "clientId": "9876"
        },
        "search": {
            "req": {
                "path": "/search/text/",
                "method": "GET",
                "params": [
                    { "key": "format", "value": "json" },
                    { "key": "fields", "value": "id,url,name,description,license,filesize,duration,username,download,previews,images,type" },
                    { "key": "page_size", "value": "%perpage%" },
                    { "key": "page", "value": "%pagenum%" },
                    { "key": "query", "value": "%query%" },
                    { "key": "token", "value": "%clientkey%" }
                ]
            },
            "res": {
                "format": "json",
                "resultCount":"count",
                "list":"results",
                "name":"name",
                "filetype":"type",
                "id":"id",
                "url":"url",
                "licenseUrl":"license",
                "description": "description",
                "author": "username",
                "authorUrl": "$https://freesound.org/people/{username}",
                "duration": "duration",
                "filesize":"filesize",
                "downloadUrl": "download",
                "previewUrl": "previews.preview-hq-mp3",
                "imageUrl": "images.waveform_m"
            }
        }
    }
}
```

## Base Structure
Each provider config files should only specify a certain media type such as `video`. If the provider provides multiple types, create one config file for each.

| Key | Type | Required | Description |
| :------------- | :------------- | :------------- | :------------- |
| name | String | yes | Name of the provider (not translatable!!!) |
| homepage | String | yes | Url pointing to the providers webpage |
| type | String | yes | one of `video`, `image`, `music`, `sound` |
| integration | String | yes | Must be `buildin` as this is the only supported value at the moment |
| clientkey | String | If OAuth2 or `%clientkey%` is used | The client key to access the api. </br>_Kdenlive has some keys build in:  `%pixabay_apikey%`, `%freesound_apikey%` and `%pexels_apikey%` will be replaced by a key for the certain provider._ |
| downloadOAuth2 | bool | no | Whether OAuth2 authentication is needed to download files
| api | Object | yes | see  [Api](#api) |

## Api
The `api` object describs the api endpoints

| Key | Type | Required | Description |
| :------------- | :------------- | :------------- | :------------- |
| root | String | yes | The apis base url (should *not* end with `/`) |
| oauth2 | Object | If [`downloadOAuth2`](#base-structure) is `true` | See [OAuth2](#oauth2) |
| search | Object | yes | See [Search](#search) |
| downloadUrls | Object | yes | See [Fetch Download Urls](#fetch-download-urls) |

### OAuth2

| Key | Type | Required | Description |
| :------------- | :------------- | :------------- | :------------- |
| authorizationUrl | String | yes | The url to authorize |
| accessTokenUrl | String | yes | The url to request an access token |
| clientId | String | yes | The clients id |

### Search
The `search` object should hold the two objects `req` and `res`.
#### Request
| Key | Type | Required | Description |
| :------------- | :------------- | :------------- | :------------- |
| path | String | yes | Path to the search endpoint (appended to [`root`](#api), should start with `/`) |
| method | String | yes | HTTP method. Only `GET` is supported at the moment |
| params | Array of Objects | no | List of HTTP params |
| header | Array of Objects| no | List HTTP headers |

The objects in the arrays in `params` and `header` should contain two fields: `key` and `value`

#### Response
| Key | Type | Required | Description |
| :------------- | :------------- | :------------- | :------------- |
| format | String | yes | Format of the response. Only `json` is supported at the moment |
| resultCount | String | yes | Number of total result |
| list | String | | Name of the key containing the list holding the search results. All of the following fields are relative to a item of this list |
| name | String | no | The items name |
| filetype | String | no | The items filetype |
| id | String | yes | Name of the key in a list element holding the items id |
| url | String | | Url to the item to be opened in a external browser |
| licenseUrl | String | | Url to the license to be opened in a external browser. If you use a [template](#templates), always use link to the english license version. Kdenlive can generate License names out of links to Creative Commons, Pexels and Pixabay Licenses for all other links the License name is "Unknown License". |
| description | String | no | Description of the item |
| author | String | no | Name of the items author |
| authorUrl | String | no | Link to the items authors page to be opened in a external browser |
| duration | String | no | Duration of the item (for video and audio)|
| width | String | no | Width of the item (for video and image) |
| height | String | no | Height of the item (for video and image) |
| downloadUrl | String | no, if `downloadUrls` or [Fetch Download Urls](#fetch-download-urls) | To be used when there is only one download url (i.e. one file version) for the item |
| downloadUrls | Object | no, if `downloadUrl` or [Fetch Download Urls](#fetch-download-urls) | To be used when there are multiple download url (i.e. one file version) for the item. See description in [table](#mutliple-download-urls) below |
| previewUrl | String | no | Url to preview file of the item |
| imageUrl | String | no | Url to image thumb of the item (for audio e.g. album cover, for video a still)|

##### Multiple Download Urls

| Key | Type | Required | Description |
| :------------- | :------------- | :------------- | :------------- |
| key | String | yes | Name of the key containing the list holding the files. The following fields are relative to a item of this list |
| url | String | yes | Name of the key in a list element holding the download url |
| name | String | yes | Label for the certain file version |

###### Example
```json
"downloadUrls": {
    "key": "video_files",
    "url":"link",
    "name":"${quality} {width}x{height}"
}
```

### Fetch Download Urls
Only necessary in special cases (e.g. https://archive.org) when no download urls are provided with the search response i.e. neither `downloadUrl` nor `downloadUrls` can be set in [Search `res`](#response)

For the `req` object see the description of the [`req` obeject for `search`](#request)

The `res` object should hold two fields: `format` (same as in [search response](#response)) and `downloadUrl` (same as in [search response](#response)) or `downloadUrls`. The field `downloadUrls` is again similar to the one in [search response](#response), but has some additional fields:

| Key | Type | Required | Description |
| :------------- | :------------- | :------------- | :------------- |
| isObject | Boolean | no | Whether the list not a normal array but a object and each subobject contains metadata about a file (e.g. `"files": { "file1": { "name": "&" }, "file2": { … }, …}`)|
| format | String | no | Format of the file |

If `isObject` is `true` there is a special case for `format`, `url` and `name`: if they have the value `&` (or `{&}` within a [template](#templates)) it is replace by the key of the parent object. In the example in the table above this means `name` will be `file1`

#### Example
```json
"downloadUrls": {
    "req": {
        "path": "/details/%id%",
        "method": "GET",
        "params": [
            { "key": "output", "value": "json"}
        ]
    },
    "res": {
        "format": "json",
        "downloadUrls": {"key":"files", "isObject":true, "url":"$http://archive.org/download/%id%{&}", "format": "format", "name":"${source} {format} {width}x{height}"}
    }
}
```
### Special Keys
Special keys are available for the following fields in [Search `res`](#response): `author`, `authorUrl`, `name`, `filetype`, `description`, `id`, `url`, `licenseUrl`, `imageUrl`, `previewUrl`, `downloadUrl`, `downloadUrls.url` and `downloadUrls.name`.

In [`res` of Fetch Download Urls](#fetch-download-urls) they are available for these fields: `downloadUrl`, `downloadUrls.format`,`downloadUrls.url` and `downloadUrls.name`
#### Placeholders
Placeholders are expressions that will be replaced by something.

For `params` and `header` these of placeholders are available.

| Placeholder | Replaced By | Hint |
| :------------- | :------------- | :------------- |
| %query% | The search query the user entered | Only for search |
| %pagenum% | Th number of the page to request results for | Only for search |
| %perpage% | Number of results that should be shown per page | |
| %shortlocale% | Short local like `en-US` (at the moment always `en-US`) | |
| %clientkey% | The clients apikey defined in [`clientkey`](#base-structure) | |
| %id% | Id of the item to fetch urls for | Only for Fetch Download Urls |

#### Templates
Templates must always start with `$`. Within a template string you can use keys like this `{username}`.

*NOTE: You can not use the general key like `author`. You must use the key of the response like `username` in the [example for freesound](#Overview)!*

##### Example

If your response looks like
```json
{
  "results": [
    {
      "name": "example",
      "files": [
        {"picname": "nice-picture", "id": "1234"},
        ...
      ]
    }
  ]
}

```
and your config like this
```json
{
  ...
  "api": {
    ...
    "search": {
      ...
      "res" {
        ...
        "downloadUrls": {
            "key": "files",
            "url":"$https://example.org/files/{picname}-{id}",
            "name":"picname"
        }
      }
    }
  }
}
```
you will finally get `https://example.org/files/nice-picture-1234` as url.
