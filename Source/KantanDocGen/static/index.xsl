<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
	<xsl:output method="html"/>

	<xsl:template match="/">
		<html>
			<head>
				<title><xsl:value-of select="/root/display_name" /></title>
				<link rel="stylesheet" type="text/css" href="static/style.css" />
			</head>
			<body>
				<div id="content_container">
					<xsl:apply-templates />
				</div>
			</body>
		</html>
	</xsl:template>

	<xsl:template match="/root">
		<a class="navbar_style"><xsl:value-of select="display_name" /></a>
		<h1 class="title_style"><xsl:value-of select="display_name" /></h1>
		<xsl:apply-templates select="classes" />
	</xsl:template>

	<xsl:template match="classes">
		<h2 class="title_style">Classes</h2>
		<table>
			<tbody>
				<xsl:apply-templates select="class">
					<xsl:sort select="display_name"/>
				</xsl:apply-templates>
			</tbody>
		</table>
	</xsl:template>

	<xsl:template match="class">
		<tr>
			<td>
				<a>
					<xsl:attribute name="href">./<xsl:value-of select="id" />/<xsl:value-of select="id" />.xml</xsl:attribute>
					<xsl:apply-templates select="display_name" />
				</a>
			</td>
		</tr>
	</xsl:template>

</xsl:stylesheet>
