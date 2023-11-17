<?xml version="1.0" encoding="UTF-8"?>
<!--
 ScummC
 Copyright (C) 2008  Alban Bedel

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
-->
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml">

  <xsl:output method='xml' version='1.0' encoding='UTF-8' indent='yes'
    doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
    doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"/>

  <!-- The whole HTML document -->
  <xsl:template match="/">
    <html>
      <head>
        <title>
          <xsl:value-of select="man/@name"/>
          <xsl:text> - </xsl:text>
          <xsl:value-of select="man/@long-name"/>
        </title>
        <link rel="stylesheet" type="text/css" href="man.css"/>
      </head>
      <body>
        <xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>

  <!-- Write the body -->
  <xsl:template match="man">
    <div class="man">
      <!-- TOC -->
      <div class="toc">
        <div class="toc-entry">
          <a href="#name">name</a>
        </div>
        <div class="toc-entry">
          <a href="#synopsis">synopsis</a>
        </div>
        <xsl:apply-templates mode="part-toc"/>
      </div>
      <!-- CONTENT -->
      <div class="part-block" id="name">
        <h1>name</h1>
        <p class="part" id="name-content">
          <span class="man-name">
            <xsl:value-of select="@name"/>
          </span>
          <xsl:text> - </xsl:text>
          <span class="man-long-name" >
            <xsl:value-of select="@long-name"/>
          </span>
        </p>
      </div>
      <div class="part-block" id="synopsis">
        <h1>synopsis</h1>
          <p class="part" id="synopsis-content">
          <xsl:apply-templates select="command" mode="synopsis"/>
        </p>
      </div>
      <xsl:apply-templates mode="part-block"/>
    </div>
  </xsl:template>

  <!-- Synopsis writing -->

  <!-- Write the synopsis for a command or parameter group -->
  <xsl:template match="command|param-group" mode="synopsis">
    <div class="{name()}-synopsis">
      <span class="{name()}-synopsis-name">
        <xsl:value-of select="@name"/>
      </span>
      <xsl:apply-templates select="file|param|param-group" mode="synopsis-param"/>
      <!-- xsl:apply-templates select="param-group" mode="synopsis"/ -->
    </div>
  </xsl:template>

  <!-- Write a parameter, filename or parameter group name  -->
  <xsl:template match="file|param|param-group" mode="synopsis-param">
    <xsl:text> </xsl:text>
    <span class="synopsis-{name()}">
      <xsl:if test="not(@required = 'true')">
        <xsl:text>[</xsl:text>
      </xsl:if>
      <span class="{name()}-name">
        <xsl:value-of select="@name"/>
      </span>
      <xsl:if test="@arg">
        <span class="{name()}-arg">
          <xsl:text> </xsl:text>
          <xsl:value-of select="@arg"/>
        </span>
      </xsl:if>
      <xsl:if test="@repeat = 'true'"> &#8230;</xsl:if>
      <xsl:if test="not(@required = 'true')">
        <xsl:text>]</xsl:text>
      </xsl:if>
    </span>
  </xsl:template>


  <!-- Parts -->


  <!-- Write the title of a part -->
  <xsl:template match="*" mode="part-title">
    <xsl:choose>
      <xsl:when test="@title"><xsl:value-of select="@title"/></xsl:when>
      <xsl:otherwise><xsl:value-of select="translate(name(),'-',' ')"/></xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- TOC entry -->
  <xsl:template match="*" mode="part-toc">
    <xsl:param name="title">
      <xsl:apply-templates select="." mode="part-title"/>
    </xsl:param>
    <div class="toc-entry">
      <a href="#{translate($title,' ','-')}">
        <xsl:value-of select="$title"/>
      </a>
    </div>
  </xsl:template>

  <!-- Part block -->
  <xsl:template match="*" mode="part-block">
    <xsl:param name="title">
      <xsl:apply-templates select="." mode="part-title"/>
    </xsl:param>
    <div class="part-block" id="{translate($title,' ','-')}">
      <h1><xsl:value-of select="$title"/></h1>
      <p class="part">
        <xsl:apply-templates/>
      </p>
    </div>
  </xsl:template>

  <!-- Command parameters -->

  <!-- TOC entry -->
  <xsl:template match="command" mode="part-toc">
    <xsl:choose>
      <xsl:when test="count(param-group) &gt; 1">
        <div class="toc-entry">
          <a href="#description">parameters</a>
          <div class="toc-entry-childs">
            <xsl:apply-templates select="param-group" mode="toc"/>
          </div>
        </div>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="param-group" mode="toc"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Write a toc entry for a parameter group -->
  <xsl:template match="param-group" mode="toc">
    <xsl:param name="base-id"/>
    <xsl:variable name="id"
      select="translate(concat($base-id,@name),' ','-')"/>
    <div class="toc-entry">
      <a href="#{$id}"><xsl:value-of select="@name"/></a>
      <xsl:if test="param-group">
        <div class="toc-entry-childs">
          <xsl:apply-templates select="param-group" mode="toc">
            <xsl:with-param name="base-id" select="concat($id,'-')"/>
          </xsl:apply-templates>
        </div>
      </xsl:if>
    </div>
  </xsl:template>

  <!-- Parameters description -->
  <xsl:template match="command" mode="part-block">
    <xsl:apply-templates/>
  </xsl:template>

  <!-- Write the doc for a parameter group -->
  <xsl:template match="param-group">
    <xsl:param name="level">1</xsl:param>
    <xsl:param name="base-id"/>
    <xsl:variable name="id"
                select="translate(concat($base-id,@name),' ','-')"/>

    <div class="part-block" id="{$id}">
      <xsl:element name="h{$level}">
        <xsl:choose>
          <xsl:when test="@title"><xsl:value-of select="@title"/></xsl:when>
          <xsl:otherwise><xsl:value-of select="@name"/></xsl:otherwise>
        </xsl:choose>
      </xsl:element>
      <p class="part" id="{$id}-content">
        <xsl:if test="description">
          <p class="param-group-description">
            <xsl:apply-templates select="description"/>
          </p>
        </xsl:if>
        <xsl:apply-templates select="param|param-group">
          <xsl:with-param name="level" select="$level+1"/>
          <xsl:with-param name="base-id" select="concat($id,'-')"/>
        </xsl:apply-templates>
      </p>
    </div>
  </xsl:template>

  <!-- Write the doc for a parameter -->
  <xsl:template match="param">
    <xsl:param name="base-id"/>
    <xsl:variable name="id"
      select="translate(concat($base-id,@name),' ','-')"/>
    <p class="param" id="{$id}">
      <div class="param-usage">
        <span class="param-name">
          <xsl:value-of select="concat('-',@name)"/>
        </span>
        <xsl:if test="@arg">
          <xsl:text> </xsl:text>
          <span class="arg-name">
            <xsl:value-of select="concat('&lt;',@arg,'&gt;')"/>
          </span>
        </xsl:if>
      </div>
      <div class="param-description">
        <!-- Do we have a short description ? -->
        <xsl:choose>
          <xsl:when test="short">
            <div class="param-description-short">
              <xsl:apply-templates select="short"/>
            </div>
            <div class="param-description-long">
              <xsl:apply-templates select="text()|*[name() != 'short']"/>
            </div>
          </xsl:when>
          <xsl:otherwise>
            <xsl:apply-templates/>
          </xsl:otherwise>
        </xsl:choose>
      </div>
    </p>
  </xsl:template>

  <!-- Write the short description -->
  <xsl:template match="short">
    <xsl:apply-templates/>
  </xsl:template>

  <!-- Create a link from command names -->
  <xsl:template match="cmd">
    <a class="cmd-name" href="{.}.xml"><xsl:value-of select="."/></a>
  </xsl:template>

  <!-- Tag to reference option names -->
  <xsl:template match="opt">
    <span class="param-name"><xsl:value-of select="."/></span>
  </xsl:template>

  <!-- Tag to reference argument names -->
  <xsl:template match="arg">
    <span class="arg-name">
      <xsl:value-of select="concat('&lt;',.,'&gt;')"/>
    </span>
  </xsl:template>

  <!-- Tag to reference the default value of the option -->
  <xsl:template match="default">
    <xsl:value-of select="ancestor::param[1]/@default"/>
  </xsl:template>

  <!-- Pass some html through  -->
  <xsl:template match="p|a|em|q|pre">
    <xsl:copy>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <!-- Kill anything not explicitly matched -->
  <xsl:template match="*"/>

</xsl:stylesheet>
