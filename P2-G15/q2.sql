SELECT D.department_name, COUNT(E.employee_id)
FROM departments D
LEFT JOIN employees E
ON D.department_id = E.department_id
GROUP BY D.department_id
ORDER BY COUNT(E.employee_id) DESC;

