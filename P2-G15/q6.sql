SELECT COUNT(E.employee_id)
FROM employees E, departments D, locations L, countries C, regions R
WHERE E.department_id = D.department_id
AND D.location_id = L.location_id
AND L.country_id = C.country_id
AND C.region_id = R.region_id
AND R.region_name = "Europe";

