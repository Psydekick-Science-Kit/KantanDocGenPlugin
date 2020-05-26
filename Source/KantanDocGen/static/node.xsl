<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
	<xsl:output method="html"/>

	<xsl:template match="/">
		<html>
			<head>
				<title><xsl:value-of select="/root/shorttitle" /></title>
				<link rel="stylesheet" type="text/css" href="../../static/style.css" />
			</head>
			<body>
				<div id="content_container">
					<xsl:apply-templates/>
				</div>
			</body>
		</html>
	</xsl:template>

	<xsl:template match="shorttitle">
		<h1 class="title_style">
			<xsl:apply-templates/>
		</h1>
	</xsl:template>

	<xsl:template match="description">
		<xsl:if test="text() != ../shorttitle">
			<div class="description">
				<xsl:apply-templates/>
			</div>
		</xsl:if>
	</xsl:template>

	<xsl:template match="category">
	</xsl:template>

	<xsl:template match="images">
		<div class="node-images">
			<img>
				<xsl:attribute name="src">
					<xsl:value-of select="./simple" />
				</xsl:attribute>
			</img>
			<img>
				<xsl:attribute name="src">
					<xsl:value-of select="./advanced" />
				</xsl:attribute>
			</img>
		</div>

	</xsl:template>

	<xsl:template match="param">
		<tr>
			<td>
				<div class="param_name title_style">
					<xsl:apply-templates select="name"/>
				</div>
				<div class="param_type">
					<xsl:apply-templates select="type"/>
				</div>
			</td>
			<td>
				<xsl:apply-templates select="description"/>
			</td>
		</tr>
	</xsl:template>

	<xsl:template name="parameters">
		<table class="parameter-table">
			<tbody>
				<xsl:apply-templates/>
			</tbody>
		</table>
	</xsl:template>

	<xsl:template match="inputs">
		<details open="open">
			<summary><h3 class="title_style">Inputs</h3></summary>
			<xsl:call-template name="parameters" />
		</details>
	</xsl:template>

	<xsl:template match="outputs">
		<details open="open">
			<summary><h3 class="title_style">Outputs</h3></summary>
			<xsl:call-template name="parameters" />
		</details>
	</xsl:template>

	<xsl:template match="root">
		<div class="navbar_style">
			<a>
				<xsl:attribute name="href">../../index.xml</xsl:attribute>
				<xsl:value-of select="docs_name" />
			</a>
			<span> &gt; </span>
			<a>
				<xsl:attribute name="href">../<xsl:value-of select="class_id" />.xml</xsl:attribute>
				<xsl:value-of select="class_name" />
			</a>
			<span> &gt; </span>
			<a><xsl:value-of select="shorttitle" /></a>
		</div>

		<xsl:apply-templates />
	</xsl:template>

	<xsl:template match="fulltitle | docs_name | class_id | class_name"/>

</xsl:stylesheet>
