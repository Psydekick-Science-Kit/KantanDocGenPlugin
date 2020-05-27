<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
	<xsl:output method="html"/>

	<xsl:variable name="uri-root">
		<xsl:for-each select="/*/path/parent">../</xsl:for-each>
	</xsl:variable>

	<xsl:template match="/">
		<html>
			<head>
				<title>
					<xsl:for-each select="/*/path/parent">
						<xsl:value-of select="name" /> |
					</xsl:for-each>
					<xsl:value-of select="/*/name" />
				</title>

				<link rel="stylesheet" type="text/css">
					<xsl:attribute name="href">
						<xsl:value-of select="$uri-root" />static/style.css
					</xsl:attribute>
				</link>

				<script>
					<xsl:attribute name="src">
						<xsl:value-of select="$uri-root" />static/script.js
					</xsl:attribute>
				</script>
				<script>
					window.onload = function(){
						DocBrowser.onPageLoad('<xsl:value-of select="$uri-root" />');
					};
				</script>
			</head>
			<body>
				<div class="sidebar">
					<xsl:call-template name="sidebar">
						<xsl:with-param name="index" select="document('../index.xml')/*" />
					</xsl:call-template>
				</div>

				<div class="content">
					<xsl:apply-templates />
				</div>
			</body>
		</html>
	</xsl:template>

	<xsl:template name="sidebar">
		<xsl:param name="index" />

		<h1><xsl:value-of select="$index/name" /></h1>
		<h2>Classes</h2>
		<xsl:apply-templates select="$index/classes" />
	</xsl:template>

	<xsl:template match="/class">
		<xsl:apply-templates select="path" />
		<h2><xsl:value-of select="name" /></h2>
		<h3>Functions</h3>
		<xsl:apply-templates select="functions" />
	</xsl:template>

	<xsl:template match="/function">
		<xsl:apply-templates select="path" />
		<h2><xsl:value-of select="path/parent[last()]/name" /></h2>
		<h3><xsl:value-of select="name" /></h3>

		<p><xsl:value-of select="description" /></p>
		<div class="node-images">
			<img>
				<xsl:attribute name="src">
					<xsl:value-of select="images/simple" />
				</xsl:attribute>
			</img>
			<img>
				<xsl:attribute name="src">
					<xsl:value-of select="images/advanced" />
				</xsl:attribute>
			</img>
		</div>


		<xsl:call-template name="parameters">
			<xsl:with-param name="paramParent" select="inputs" />
		</xsl:call-template>

		<xsl:call-template name="parameters">
			<xsl:with-param name="paramParent" select="outputs" />
		</xsl:call-template>

	</xsl:template>

	<xsl:template match="path">
		<ul class="breadcrumb">
			<xsl:for-each select="parent">
				<li>
					<a>
						<xsl:attribute name="href">
							<xsl:choose>
								<xsl:when test="contains(./uri, 'index.xml')">
									<xsl:value-of select="substring-before(./uri, 'index.xml')" />
								</xsl:when>
								<xsl:otherwise>
									<xsl:value-of select="./uri" />
								</xsl:otherwise>
							</xsl:choose>
						</xsl:attribute>
						<xsl:value-of select="./name" />
					</a>
				</li>
			</xsl:for-each>
			<li>
				<xsl:value-of select="../name" />
			</li>
		</ul>
	</xsl:template>

	<xsl:template match="classes">
		<!-- Sidebar -->
		<xsl:for-each select="class">
			<xsl:sort select="name"/>

			<details>
				<xsl:attribute name="onToggle">
					DocBrowser.toggleClassSidebar(this, '<xsl:value-of select="./id" />');
				</xsl:attribute>
				<summary>
					<a>
						<xsl:attribute name="href">
							<xsl:value-of select="concat($uri-root, ./id, '/')" />
						</xsl:attribute>
						<xsl:value-of select="name" />
					</a>
				</summary>


			</details>
		</xsl:for-each>
	</xsl:template>

	<xsl:template match="functions">
		<table>
			<xsl:for-each select="function">
				<xsl:sort select="name"/>
				<tr>
					<td>
						<a>
							<xsl:attribute name="href"><xsl:value-of select="concat(id, '/index.xml')" /></xsl:attribute>
							<xsl:apply-templates select="name" />
						</a>
					</td>
					<td>
						<xsl:value-of select="description" />
					</td>
				</tr>
			</xsl:for-each>
		</table>
	</xsl:template>

	<xsl:template name="parameters">
		<xsl:param name="paramParent" />

		<details open="open">
			<summary><h3><xsl:value-of select="name($paramParent)" /></h3></summary>

			<table class="parameter-table">
				<tbody>
					<xsl:for-each select="$paramParent/param">
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
					</xsl:for-each>
				</tbody>
			</table>
		</details>

	</xsl:template>

</xsl:stylesheet>
