SELECT C.country_name
FROM countries C, regions R
WHERE C.region_id = R.region_id
AND R.region_name = "Europe";

