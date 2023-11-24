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
<!-- Add entities for new-line and quote -->
<!DOCTYPE xsl [
  <!ENTITY nl "&#x0A;">
  <!ENTITY q  "&#x22;">
]>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml">

  <xsl:output method='text' version='1.0' encoding='UTF-8' indent='yes'/>
  <xsl:strip-space elements="*"/>

  <xsl:template match="/">
    <xsl:text>/* This file was generated, do not edit. */&nl;</xsl:text>
    <xsl:apply-templates select="man/command"/>
  </xsl:template>

  <xsl:template match="command">
    <!-- Write the parameters definitions -->
    <xsl:apply-templates select="." mode="desc-groups"/>

    <!-- Write the help definition -->
    <xsl:text>static scc_help_t </xsl:text>
    <xsl:apply-templates select="@name" mode="c-sym"/>
    <xsl:text>_help = {&nl;</xsl:text>

    <xsl:text>  .name = &q;</xsl:text>
    <xsl:apply-templates select="@name" mode="c-string"/>
    <xsl:text>&q;,&nl;</xsl:text>

    <xsl:text>  .usage = &q;</xsl:text>
    <xsl:apply-templates mode="usage"/>
    <xsl:text>&q;,&nl;</xsl:text>

    <xsl:text>  .param_help = </xsl:text>
    <xsl:apply-templates select="@name" mode="c-sym"/>
    <xsl:text>_param_help&nl;</xsl:text>

    <xsl:text>};&nl;</xsl:text>
  </xsl:template>

  <xsl:template match="*"/>

  <xsl:template match="*" mode="desc"/>

  <!-- Write a parameters list -->
  <xsl:template match="command|param-group" mode="desc-groups">
    <!-- Write the child lists first -->
    <xsl:apply-templates select="param-group" mode="desc-groups"/>
    <!-- Then write this list -->
    <xsl:text>static scc_param_help_t </xsl:text>
    <xsl:apply-templates select="@name" mode="c-sym"/>
    <xsl:text>_param_help[] = {&nl;  </xsl:text>
    <xsl:apply-templates select="param-group|param" mode="desc"/>
    <xsl:text>{}&nl;};&nl;&nl;</xsl:text>
  </xsl:template>

  <!-- Parameter entry for a sub group -->
  <xsl:template match="param-group" mode="desc">
    <xsl:text>{&nl;</xsl:text>

    <xsl:text>    .name = &q;</xsl:text>
    <xsl:apply-templates select="@name" mode="c-string-up"/>
    <xsl:text>&q;,&nl;</xsl:text>

    <xsl:text>    .group = </xsl:text>
    <xsl:apply-templates select="@name" mode="c-sym"/>
    <xsl:text>_param_help&nl;  },</xsl:text>
  </xsl:template>

  <!-- Parameter description -->
  <xsl:template match="param" mode="desc">
    <xsl:text>{&nl;</xsl:text>

    <xsl:text>    .name = &q;</xsl:text>
    <xsl:apply-templates select="@name" mode="c-string"/>
    <xsl:text>&q;,&nl; </xsl:text>

    <xsl:text>   .arg = </xsl:text>
    <xsl:choose>
      <xsl:when test="@arg">
        <xsl:text>&q;</xsl:text>
        <xsl:apply-templates select="@arg" mode="c-string"/>
        <xsl:text>&q;,&nl;</xsl:text>
      </xsl:when>
      <xsl:otherwise>NULL,&nl;</xsl:otherwise>
    </xsl:choose>

    <xsl:text>    .dfault = </xsl:text>
    <xsl:choose>
      <xsl:when test="@default">
        <xsl:text>&q;</xsl:text>
        <xsl:apply-templates select="@default" mode="c-string"/>
        <xsl:text>&q;,&nl;</xsl:text>
      </xsl:when>
      <xsl:otherwise>NULL,&nl;</xsl:otherwise>
    </xsl:choose>


    <xsl:text>    .desc = </xsl:text>
    <xsl:text>&q;</xsl:text>
    <xsl:apply-templates select="." mode="desc-txt"/>
    <xsl:text>&q;&nl;  },</xsl:text>

  </xsl:template>

  <!-- Write the description of a parameter -->
  <xsl:template match="param" mode="desc-txt">
    <xsl:choose>
      <xsl:when test="short">
        <xsl:apply-templates select="short" mode="c-string"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates mode="c-string"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Write the usage line -->
  <xsl:template match="file|param|param-group" mode="usage">
    <xsl:if test="position() &gt; 1">
      <xsl:text> </xsl:text>
    </xsl:if>
    <xsl:if test="not(@required = 'true')">
      <xsl:text>[</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="@name" mode="usage"/>
    <xsl:if test="@arg">
      <xsl:text> </xsl:text>
      <xsl:apply-templates select="@arg" mode="c-string"/>
    </xsl:if>
    <xsl:if test="@repeat = 'true'"> ...</xsl:if>
    <xsl:if test="not(@required = 'true')">
      <xsl:text>]</xsl:text>
    </xsl:if>
  </xsl:template>

  <!-- Write usage parts with c string escaping -->
  <xsl:template match="*/@name" mode="usage">
    <xsl:apply-templates select="." mode="c-string"/>
  </xsl:template>

  <!-- Write group names in upper case -->
  <xsl:template match="param-group/@name" mode="usage">
    <xsl:apply-templates select="." mode="c-string-up"/>
  </xsl:template>

  <!-- Add a - prefix to parameter names -->
  <xsl:template match="param/@name" mode="usage">
    <xsl:text>-</xsl:text>
    <xsl:apply-templates select="." mode="c-string"/>
  </xsl:template>

  <!-- Write something as a valid c string -->
  <xsl:template match="*" mode="c-string">
    <xsl:apply-templates mode="c-string"/>
  </xsl:template>

  <!-- Write text and attributes values as normalized c string -->
  <xsl:template match="text()|@*" mode="c-string">
    <xsl:call-template name="c-string-escape">
      <xsl:with-param name="str" select="normalize-space(.)"/>
    </xsl:call-template>
  </xsl:template>

  <!-- Write text and attributes values as uppercase normalized c string -->
  <xsl:template match="@*|*" mode="c-string-up">
    <xsl:call-template name="c-string-escape">
      <xsl:with-param name="str" select="translate(.,'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
    </xsl:call-template>
  </xsl:template>

  <!-- Escape " and \ -->
  <xsl:template name="c-string-escape">
    <xsl:param name="str"/>
    <xsl:choose>
      <xsl:when test="contains($str,'&q;')">
        <xsl:call-template name="c-string-escape">
          <xsl:with-param name="str" select="substring-before($str,'&q;')"/>
        </xsl:call-template>
        <xsl:text>\&q;</xsl:text>
        <xsl:call-template name="c-string-escape">
          <xsl:with-param name="str" select="substring-after($str,'&q;')"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($str,'\')">
        <xsl:call-template name="c-string-escape">
          <xsl:with-param name="str" select="substring-before($str,'\')"/>
        </xsl:call-template>
        <xsl:text>\\</xsl:text>
        <xsl:call-template name="c-string-escape">
          <xsl:with-param name="str" select="substring-after($str,'\')"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$str"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Create a valid symbol name out of some text -->
  <xsl:template match="text()|@*|*" mode="c-sym">
    <xsl:call-template name="c-sym-escape">
      <xsl:with-param name="str" select="normalize-space(.)"/>
    </xsl:call-template>
  </xsl:template>

  <!-- Convert anything not in a-Z0-9_ to _ -->
  <xsl:template name="c-sym-escape">
    <xsl:param name="str"/>
    <xsl:choose>
      <xsl:when test="contains('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_',substring($str,0,2))">
        <xsl:value-of select="substring($str,0,2)"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>_</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="substring($str,2)">
      <xsl:call-template name="c-sym-escape">
        <xsl:with-param name="str" select="substring($str,2)"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>


</xsl:stylesheet>
