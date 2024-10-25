SELECT D.department_name, AVG(J.max_salary)
FROM departments D, employees E, jobs J
WHERE D.department_id = E.department_id
AND E.job_id = J.job_id
GROUP BY D.department_id
HAVING AVG(J.max_salary) > 8000;

