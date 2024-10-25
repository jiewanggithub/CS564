SELECT D.department_name, E.salary
FROM employees E, departments D
WHERE E.department_id = D.department_id
ORDER BY E.salary DESC
LIMIT 1;

