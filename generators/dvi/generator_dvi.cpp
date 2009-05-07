/***************************************************************************
 *   Copyright (C) 2006 by Luigi Toscano <luigi.toscano@tiscali.it>        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <okular/core/action.h>
#include <okular/core/document.h>
#include <okular/core/fileprinter.h>
#include <okular/core/page.h>
#include <okular/core/sourcereference.h>
#include <okular/core/textpage.h>
#include <okular/core/utils.h>

#include "generator_dvi.h"
#include "dviFile.h"
#include "dviPageInfo.h"
#include "dviRenderer.h"
#include "pageSize.h"
#include "dviexport.h"

#include <qapplication.h>
#include <qstring.h>
#include <qurl.h>
#include <qvector.h>
#include <qstack.h>

#include <kaboutdata.h>
#include <kdebug.h>
#include <klocale.h>
#include <ktemporaryfile.h>

static const int DviDebug = 4713;

static KAboutData createAboutData()
{
    KAboutData aboutData(
         "okular_dvi",
         "okular_dvi",
         ki18n( "DVI Backend" ),
         "0.1",
         ki18n( "A DVI file renderer" ),
         KAboutData::License_GPL,
         ki18n( "© 2006 Luigi Toscano" )
    );
    return aboutData;
}

OKULAR_EXPORT_PLUGIN( DviGenerator, createAboutData() )

DviGenerator::DviGenerator( QObject *parent, const QVariantList &args ) : Okular::Generator( parent, args ),
  m_docInfo( 0 ), m_docSynopsis( 0 ), ready( false ), m_dviRenderer( 0 )
{
    setFeature( TextExtraction );
    setFeature( PrintPostscript );
    if ( Okular::FilePrinter::ps2pdfAvailable() )
        setFeature( PrintToFile );
}

bool DviGenerator::loadDocument( const QString & fileName, QVector< Okular::Page * > &pagesVector )
{
    //kDebug(DviDebug) << "file:" << fileName;
    KUrl base( fileName );

    m_dviRenderer = new dviRenderer();
    if ( ! m_dviRenderer->setFile( fileName, base ) )
    {
        delete m_dviRenderer;
        m_dviRenderer = 0;
        return false;
    }

    m_dviRenderer->setParentWidget( document()->widget() );

    kDebug(DviDebug) << "# of pages:" << m_dviRenderer->dviFile->total_pages;

    m_resolution = Okular::Utils::dpiY();
    loadPages( pagesVector );

    ready = true;
    return true;
}

bool DviGenerator::doCloseDocument()
{
    delete m_docInfo;
    m_docInfo = 0;
    delete m_docSynopsis;
    m_docSynopsis = 0;
    delete m_dviRenderer;
    m_dviRenderer = 0;

    ready = false;

    return true;
}

bool DviGenerator::canGeneratePixmap () const
{
    return ready;
}

void DviGenerator::fillViewportFromAnchor( Okular::DocumentViewport &vp,
                                           const Anchor &anch, int pW, int pH ) 
{
    vp.pageNumber = anch.page - 1;


    SimplePageSize ps = m_dviRenderer->sizeOfPage( vp.pageNumber );
    double resolution = 0;

    resolution = (double)(pW)/ps.width().getLength_in_inch();

    double py = (double)anch.distance_from_top.getLength_in_inch()*resolution + 0.5; 
 
    vp.rePos.normalizedX = 0.5;
    vp.rePos.normalizedY = py/(double)pH;
    vp.rePos.enabled = true;
    vp.rePos.pos = Okular::DocumentViewport::Center;
}

QLinkedList<Okular::ObjectRect*> DviGenerator::generateDviLinks( const dviPageInfo *pageInfo )
{
    QLinkedList<Okular::ObjectRect*> dviLinks;

    int pageWidth = pageInfo->width, pageHeight = pageInfo->height;
    
    foreach( const Hyperlink &dviLink, pageInfo->hyperLinkList )
    {
        QRect boxArea = dviLink.box;
        double nl = (double)boxArea.left() / pageWidth,
               nt = (double)boxArea.top() / pageHeight,
               nr = (double)boxArea.right() / pageWidth,
               nb = (double)boxArea.bottom() / pageHeight;
        
        QString linkText = dviLink.linkText;
        if ( linkText.startsWith("#") )
            linkText = linkText.mid( 1 );
        Anchor anch = m_dviRenderer->findAnchor( linkText );

        Okular::Action *okuLink = 0;

        /* distinguish between local (-> anchor) and remote links */
        if (anch.isValid())
        {
            /* internal link */
            Okular::DocumentViewport vp;
            fillViewportFromAnchor( vp, anch, pageWidth, pageHeight );

            okuLink = new Okular::GotoAction( "", vp );
        }
        else
        {
            okuLink = new Okular::BrowseAction( dviLink.linkText );
        }
        if ( okuLink ) 
        {
            Okular::ObjectRect *orlink = new Okular::ObjectRect( nl, nt, nr, nb, 
                                        false, Okular::ObjectRect::Action, okuLink );
            dviLinks.push_front( orlink );
        }

    }
    return dviLinks; 
}

void DviGenerator::generatePixmap( Okular::PixmapRequest *request )
{

    dviPageInfo *pageInfo = new dviPageInfo();
    pageSize ps;

    pageInfo->width = request->width();
    pageInfo->height = request->height();

    pageInfo->pageNumber = request->pageNumber() + 1;

//  pageInfo->resolution = m_resolution;

    SimplePageSize s = m_dviRenderer->sizeOfPage( pageInfo->pageNumber );

/*   if ( s.width() != pageInfo->width) */
    //   if (!useDocumentSpecifiedSize)
    //    s = userPreferredSize;

    if (s.isValid())
    {
        ps = s; /* it should be the user specified size, if any, instead */
    }

    pageInfo->resolution = (double)(pageInfo->width)/ps.width().getLength_in_inch();

#if 0
    kDebug(DviDebug) << *request
    << ", res:" << pageInfo->resolution << " - (" << pageInfo->width << ","
    << ps.width().getLength_in_inch() << ")," << ps.width().getLength_in_mm()
    << endl;
#endif

    if ( m_dviRenderer )
    {
        m_dviRenderer->drawPage( pageInfo );

        if ( ! pageInfo->img.isNull() )
        {
            kDebug(DviDebug) << "Image OK";

            if ( !request->page()->isBoundingBoxKnown() )
                updatePageBoundingBox( request->page()->number(), Okular::Utils::imageBoundingBox( &(pageInfo->img) ) );

            request->page()->setPixmap( request->id(), new QPixmap( QPixmap::fromImage( pageInfo->img ) ) );

            request->page()->setObjectRects( generateDviLinks( pageInfo ) );
        }
    }

    ready = true;

    delete pageInfo;

    signalPixmapRequestDone( request );
}

Okular::TextPage* DviGenerator::textPage( Okular::Page *page )
{
    kDebug(DviDebug);
    dviPageInfo *pageInfo = new dviPageInfo();
    pageSize ps;
    
    pageInfo->width=page->width();
    pageInfo->height=page->height();

    pageInfo->pageNumber = page->number() + 1;

    pageInfo->resolution = m_resolution;
    SimplePageSize s = m_dviRenderer->sizeOfPage( pageInfo->pageNumber );
    pageInfo->resolution = (double)(pageInfo->width)/ps.width().getLength_in_inch();

    // get page text from m_dviRenderer
    Okular::TextPage *ktp = 0;
    if ( m_dviRenderer )
    {
        m_dviRenderer->getText( pageInfo );

        ktp = extractTextFromPage( pageInfo );
    }
    delete pageInfo;
    return ktp;
}

Okular::TextPage *DviGenerator::extractTextFromPage( dviPageInfo *pageInfo )
{
    QList<Okular::TextEntity*> textOfThePage;

    QVector<TextBox>::ConstIterator it = pageInfo->textBoxList.constBegin();
    QVector<TextBox>::ConstIterator itEnd = pageInfo->textBoxList.constEnd();
    QRect tmpRect;

    int pageWidth = pageInfo->width, pageHeight = pageInfo->height;

    for ( ; it != itEnd ; ++it )
    {
        TextBox curTB = *it;
 
#if 0
        kDebug(DviDebug) << "orientation: " << orientation
                 << ", curTB.box: " << curTB.box
                 << ", tmpRect: " << tmpRect 
                 << ", ( " << pageWidth << "," << pageHeight << " )" 
               <<endl;
#endif
        textOfThePage.push_back( new Okular::TextEntity( curTB.text,
              new Okular::NormalizedRect( curTB.box, pageWidth, pageHeight ) ) );
    }

    Okular::TextPage* ktp = new Okular::TextPage( textOfThePage );

    return ktp;
}

const Okular::DocumentInfo *DviGenerator::generateDocumentInfo()
{
    if ( m_docInfo )
        return m_docInfo;

    m_docInfo = new Okular::DocumentInfo();

    m_docInfo->set( Okular::DocumentInfo::MimeType, "application/x-dvi" );

    if ( m_dviRenderer && m_dviRenderer->dviFile )
    {
        dvifile *dvif = m_dviRenderer->dviFile;

        // read properties from dvif
        //m_docInfo->set( "filename", dvif->filename, i18n("Filename") );
        m_docInfo->set( "generatorDate", dvif->generatorString,
                       i18n("Generator/Date") );
        m_docInfo->set( Okular::DocumentInfo::Pages, QString::number( dvif->total_pages ) );
    }
    return m_docInfo;
}

const Okular::DocumentSynopsis *DviGenerator::generateDocumentSynopsis()
{
    if ( m_docSynopsis )
        return m_docSynopsis;

    m_docSynopsis = new Okular::DocumentSynopsis();

    QVector<PreBookmark> prebookmarks = m_dviRenderer->getPrebookmarks();

    if ( prebookmarks.isEmpty() ) 
        return m_docSynopsis;

    QStack<QDomElement> stack;

    QVector<PreBookmark>::ConstIterator it = prebookmarks.constBegin();
    QVector<PreBookmark>::ConstIterator itEnd = prebookmarks.constEnd();
    for( ; it != itEnd; ++it ) 
    {
        QDomElement domel = m_docSynopsis->createElement( (*it).title );
        Anchor a = m_dviRenderer->findAnchor((*it).anchorName);
        if ( a.isValid() )
        {
            Okular::DocumentViewport vp;
 
            const Okular::Page *p = document()->page( a.page - 1 );

            fillViewportFromAnchor( vp, a, (int)p->width(), (int)p->height() );
            domel.setAttribute( "Viewport", vp.toString() );
        }
        if ( stack.isEmpty() )
            m_docSynopsis->appendChild( domel );
        else 
        {
            stack.top().appendChild( domel );
            stack.pop();
        }
        for ( int i = 0; i < (*it).noOfChildren; ++i )
            stack.push( domel );
    }

    return m_docSynopsis;
}

void DviGenerator::loadPages( QVector< Okular::Page * > &pagesVector )
{
    QSize pageRequiredSize;

    int numofpages = m_dviRenderer->dviFile->total_pages;
    pagesVector.resize( numofpages );

    //kDebug(DviDebug) << "resolution:" << m_resolution << ", dviFile->preferred?";

    /* get the suggested size */
    if ( m_dviRenderer->dviFile->suggestedPageSize )
    {
        pageRequiredSize = m_dviRenderer->dviFile->suggestedPageSize->sizeInPixel(
               m_resolution );
    }
    else
    {
        pageSize ps;
        pageRequiredSize = ps.sizeInPixel( m_resolution );
    }

    for ( int i = 0; i < numofpages; ++i )
    {
        //kDebug(DviDebug) << "getting status of page" << i << ":";

        if ( pagesVector[i] )
        {
            delete pagesVector[i];
        }

        Okular::Page * page = new Okular::Page( i,
                                        pageRequiredSize.width(),
                                        pageRequiredSize.height(),
                                        Okular::Rotation0 );

        pagesVector[i] = page;
    }
    kDebug(DviDebug) << "pagesVector successfully inizialized!";

    // filling the pages with the source references rects
    const QVector<DVI_SourceFileAnchor>& sourceAnchors = m_dviRenderer->sourceAnchors();
    QVector< QLinkedList< Okular::SourceRefObjectRect * > > refRects( numofpages );
    foreach ( const DVI_SourceFileAnchor& sfa, sourceAnchors )
    {
        if ( sfa.page < 1 || (int)sfa.page > numofpages )
            continue;

        Okular::NormalizedPoint p( -1.0, (double)sfa.distance_from_top.getLength_in_pixel( Okular::Utils::dpiY() ) / (double)pageRequiredSize.height() );
        Okular::SourceReference * sourceRef = new Okular::SourceReference( sfa.fileName, sfa.line );
        refRects[ sfa.page - 1 ].append( new Okular::SourceRefObjectRect( p, sourceRef ) );
    }
    for ( int i = 0; i < refRects.size(); ++i )
        if ( !refRects.at(i).isEmpty() )
            pagesVector[i]->setSourceReferences( refRects.at(i) );
}

bool DviGenerator::print( QPrinter& printer )
{
    // Create tempfile to write to
    KTemporaryFile tf;
    tf.setSuffix( ".ps" );
    if ( !tf.open() )
        return false;

    QList<int> pageList = Okular::FilePrinter::pageList( printer, 
                                 m_dviRenderer->totalPages(),
                                 document()->bookmarkedPageList() );
    QString pages;
    QStringList printOptions;
    // List of pages to print.
    foreach ( int p, pageList )
    {
        pages += QString(",%1").arg(p);
    }
    if ( !pages.isEmpty() )
        printOptions << "-pp" << pages.mid(1);

    QEventLoop el;
    m_dviRenderer->setEventLoop( &el );
    m_dviRenderer->exportPS( tf.fileName(), printOptions, &printer );

    tf.close();

    // Error messages are handled by the generator - ugly, but it works.
    return true; 
}


#include "generator_dvi.moc"
