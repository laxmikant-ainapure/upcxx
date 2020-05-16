#!/bin/bash

# This script is a helper that can be used to link CI build results from anywhere into the main repo
# The user calling must have write access to the main repo and an app password for their account with write perms

CI_COMMIT_SHA=$1
CI_STATE=$2
CI_JOB_URL=$3
CI_JOB_NAME=$4

if [[ -z "$BITBUCKET_APP_PASSWORD" || -z "$BITBUCKET_USER" || -z "$CI_COMMIT_SHA" || -z "$CI_STATE" ]]; then
  echo "Usage: env BITBUCKET_USER=yourname BITBUCKET_APP_PASSWORD=pass $0 <commit_id> <SUCCESSFUL|INPROGRESS|FAILED> [<url> <jobname>]"
  exit 1
fi
case "$CI_STATE" in 
 SUCCESSFUL | INPROGRESS | FAILED)  ;;
 *) echo "Error <state> must be one of: SUCCESSFUL, INPROGRESS, FAILED" ; exit 1
esac
if [[ -z "$CI_JOB_NAME" ]] ; then
  CI_JOB_NAME="pipeline"
fi

URL="https://api.bitbucket.org/2.0/repositories/berkeleylab/upcxx/commit/${CI_COMMIT_SHA}/statuses/build"
QUERY="{ \"state\": \"$CI_STATE\""
QUERY+=", \"key\": \"$CI_JOB_NAME\""
if [[ -n "$CI_JOB_URL" ]] ; then
  QUERY+=", \"url\": \"$CI_JOB_URL\""
fi
QUERY+=" }"

echo "Posting to $URL:"
echo "  $QUERY"

curl -sS $URL \
      -u "$BITBUCKET_USER:$BITBUCKET_APP_PASSWORD" -H "Content-Type: application/json"  \
      -X POST -d "$QUERY"
