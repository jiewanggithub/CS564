SELECT COUNT(E.employee_id)
FROM departments D, employees E
WHERE D.department_id = E.department_id
AND D.department_name = "Shipping";

