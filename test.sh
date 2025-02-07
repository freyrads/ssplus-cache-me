#!/bin/bash

echo -E spam request to GET /cache/:key with memory cache unpopulated
echo
idx=20

while ((idx--));
do
  time curl localhost:3000/cache/$idx
done

echo -E ================================================================================
echo
echo -E spam request to POST /cache/get-or-set with different key for each request
echo
idx=10

while ((idx--));
do
  time curl -X POST localhost:3000/cache/get-or-set --data '{"key": "'$idx'", "value": "suog" }'
done

echo -E ================================================================================
echo
echo -E spam request to POST /cache/get-or-set with the previously cached key for each request
echo
idx=20

while ((idx--));
do
  time curl -X POST localhost:3000/cache/get-or-set --data '{"key": "'$idx'", "value": "suog" }'
done

echo -E ================================================================================
echo
echo -E spam request to POST /cache with the different key for each request with ttl 10s
echo
idx=20

while ((idx--));
do
  time curl -X POST localhost:3000/cache --data '{"key": "'$idx'", "value": "suog", "ttl": 10000 }'
done

echo -E ================================================================================
echo
echo -E spam request to GET /cache/:key with the previously cached key for each request
echo
idx=20

while ((idx--));
do
  time curl localhost:3000/cache/$idx
done

echo -E ================================================================================
echo
echo -E spam request to DELETE /cache/:key to the first 10 entry
echo
idx=10

while ((idx--));
do
  time curl -X DELETE localhost:3000/cache/$idx
done
