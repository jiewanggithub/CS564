SELECT employee_id
FROM employees
WHERE employee_id NOT IN (
	SELECT E.employee_id
	FROM employees E, dependents D
	WHERE E.employee_id = D.employee_id
	GROUP BY E.employee_id
);

