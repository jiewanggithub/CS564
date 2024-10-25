SELECT M.employee_id, E.salary
FROM employees E, employees M
WHERE E.manager_id = M.employee_id
ORDER BY E.salary ASC
LIMIT 1;

