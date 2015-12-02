#!/bin/bash
echo "Content-type: text/html; charset=iso-8859-1"
echo "Refresh: 5"
echo

cat <<EOF
<html>
<head>
	<title>Kitchen palm</title>
	<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
</head>
<body>
EOF
echo  '<br>'

exec < ../addrs
while read adr name
do
	v=0
	../get $adr && v=1
	echo $v $name '<br>'
done

cat <<EOF
</body>
</html>
EOF
