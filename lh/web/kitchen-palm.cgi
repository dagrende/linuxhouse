#!/bin/bash
echo "Content-type: text/html; charset=UTF-8"
echo "Refresh: 5"
echo

cat <<EOF
<html>
<head>
	<title>Kitchen palm</title>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<body>
EOF
cd "`dirname $0`"
if [[ `cat ../post-in-postbox` = "1" ]]; then
echo  Post!
else
echo  Ingen post.
fi
echo  '<br>'
echo
echo Temperatur '<br>'
echo "&nbsp;inne" `../get 100EE90700000047` grader C '<br>'
echo "&nbsp;ute" `../get 10E4790600000099` grader C '<br>'
echo
if ../get 05F3A70200000087 && ../get 057CAC160000000F && ../get 0596B11600000079; then
	# its locked
	echo "Låst"
else
	# its not locked
	echo -n "Ej låst i"
	../get 05F3A70200000087 || echo -n " datarum <br>"
	../get 057CAC160000000F || echo -n " garage <br>"
	../get 0596B11600000079 || echo -n " entre <br>"
	echo
fi
../get 0549A40200000001 && echo Motorvärmaren är på '<br>'
echo "</body></html>"