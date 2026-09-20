# GUÍA PASO A PASO PARA PRESENTAR LA PATENTE PROVISIONAL EN LA USPTO (EE.UU.)

Esta guía detalla el procedimiento exacto para registrar la solicitud provisional de patente ante la **United States Patent and Trademark Office (USPTO)** para obtener el estatus oficial **"Patent Pending"** de inmediato.

---

## 1. Documentos Preparados en el Repositorio

Ya tienes listos los dos documentos técnicos obligatorios en el repositorio:
1. **Memoria Técnica Descriptiva**: [`docs/patent/PROVISIONAL_PATENT_SPECIFICATION.md`](file:///C:/symbols/docs/patent/PROVISIONAL_PATENT_SPECIFICATION.md)
   *(Contiene el título formal, antecedentes, resumen de la invención, descripción técnica detallada, reivindicaciones y abstract).*
2. **Figuras y Diagramas Formales**: [`docs/patent/DIAGRAMS.md`](file:///C:/symbols/docs/patent/DIAGRAMS.md)
   *(Contiene FIG 1 a FIG 5 con la arquitectura de bloques, bucle de autocuración, subprocess anti-deadlock, blast radius y rollback atómico).*

> **Paso Previo (Generar PDFs)**:
> Abre ambos archivos en tu editor (o en el navegador / visor Markdown) y selecciona **Imprimir -> Guardar como PDF**:
> - `Specification.pdf`
> - `Drawings.pdf`

---

## 2. Acceso al Portal Oficial de la USPTO

1. Entra en el portal oficial: **[https://patentcenter.uspto.gov/](https://patentcenter.uspto.gov/)**
2. Haz clic en **"File a patent application"** (o "File as unregistered guest" si no tienes cuenta MyUSPTO creada aún).
   *(Nota: Se recomienda crear una cuenta gratuita en `uspto.gov` con tu correo electrónico para tener acceso permanente al expediente y recibir las notificaciones electrónicas al instante).*

---

## 3. Completar los Datos de la Solicitud Online

El sistema te guiará por una serie de pantallas:

### A. Tipo de Solicitud (Application Type)
- **Application Type**: Selecciona **Provisional**.
- **Subject Matter**: **Utility**.

### B. Datos de la Invención
- **Title of Invention**:  
  `DETERMINISTIC SYMBOLIC ARTIFICIAL INTELLIGENCE SYSTEM AND CLOSED-LOOP ABDUCTIVE METHOD FOR AUTONOMOUS SOFTWARE SYNTHESIS, VERIFICATION, AND REPAIR`

### C. Datos del Inventor (Applicant / Inventor)
- **Given Name**: `Antonio`
- **Family Name**: `Linares`
- **City**: `Marbella` (o tu ciudad de residencia fiscal)
- **State/Province**: `Málaga` / `Madrid`
- **Country**: `Spain (ES)`
- **Email**: Tu correo habitual de contacto.

### D. Declaración de Estatus de Entidad (Entity Status - Clave para el descuento)
La USPTO ofrece descuentos drásticos a inventores independientes:
- Selecciona **"Micro Entity"** (descuento del 80% en las tasas oficiales).
- El sistema te mostrará el formulario electrónico **PTO/SB/15A** (Certification of Micro Entity Status on a Gross Income Basis).
- Marcas las casillas que confirman:
  1. No haber sido nombrado como inventor en más de 4 patentes estadounidenses previas.
  2. No superar el umbral de ingresos brutos anuales fijado por la USPTO (~$223,000 USD).
  3. No haber cedido ni estar obligado a ceder la invención a una gran empresa.

---

## 4. Subida de Archivos (Upload Documents)

En la pantalla de subida de archivos:
1. Sube `Specification.pdf` y selecciona en el desplegable de categoría: **"Specification"**.
2. Sube `Drawings.pdf` y selecciona en el desplegable: **"Drawings - other than black and white line drawings"** o **"Drawings"**.
3. El sistema verificará automáticamente el PDF y mostrará un visto verde en la validación.

---

## 5. Pago de la Tasa Oficial y Descarga del Recibo

1. **Importe**: Con la condición de Micro Entity, la tasa oficial de presentación provisional es de aproximadamente **~$60 USD** (unos 55 €).
2. **Método de pago**: Tarjeta de crédito o débito internacional a través de la pasarela segura del gobierno de EE.UU. (Pay.gov integrada en el portal).
3. **Confirmación Inmediata**:
   - En el instante en que se procesa el pago, la pantalla generará el **"Electronic Filing Receipt"** (Recibo Oficial Electrónico de Presentación).
   - Este recibo contiene:
     - **Application Number** (número oficial de patente provisional de EE.UU., ej: `63/xxx,xxx`).
     - **Filing Date & Timestamp** (Fecha y hora legal exacta de la prioridad).

---

## 6. Efectos Legales Obtenidos Inmediatamente

Desde el momento en que tengas tu *Electronic Filing Receipt*:
1. **Derecho a usar "Patent Pending"**: Puedes incluir legalmente en tu web, README de GitHub y notas de prensa:  
   `"U.S. Provisional Patent Pending Application No. 63/xxx,xxx"`
2. **12 Meses de Prioridad Internacional**: Tienes 1 año completo para:
   - Extender la patente definitiva a nivel mundial mediante el Tratado de Cooperación de Patentes (**PCT**), cubriendo EE.UU., Europa (EPO), Japón, etc.
   - Negociar licencias, acuerdos con inversores o empresas tecnológicas con total protección y confidencialidad.
