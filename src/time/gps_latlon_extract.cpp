//
// Author: Alexander Sholohov <ra9yer@yahoo.com>
//
// License: MIT
//
#include "gps_config.h"
#include "gps_latlon_extract.h"

#include <string.h>
#include <inttypes.h>


#define ENSURE_COMMA do{char ch=m_tmpBuf[pos++]; if(ch!=',') return; }while(0)
#define ENSURE_POINT do{char ch=m_tmpBuf[pos++]; if(ch!='.') return; }while(0)
#define ENSURE_DIGIT_AND_CATCH(DST) do{char ch=m_tmpBuf[pos++]; if(ch<'0' || ch>'9') return; DST = ch; }while(0)
#define ENSURE_EITHER_AND_CATCH(A, B, DST) do{char ch=m_tmpBuf[pos++]; if(ch!=A && ch!=B) return; DST = ch;}while(0)

//---------------------------------------------------------------------------------
GpsLatLonExtract::GpsLatLonExtract()
    : m_state(stWaitLine)
    , m_numCommas(0)
    , m_pos(0)
    , m_validLatLonPresent( false )
{
}

#ifdef USE_GPRMC
//---------------------------------------------------------------------------------
void GpsLatLonExtract::onCharReceived(char ch)
{
    for(size_t idx = 1; idx < 6; idx++)
    {
        m_rmc[idx - 1] = m_rmc[idx];
    }
    m_rmc[5] = ch;

    if(strncmp( m_rmc, "$GPRMC", 6 ) == 0)
    {
        m_state = stWaitStatusChar;
        m_numCommas = 0;
        return;
    }

    if(m_state == stWaitStatusChar)
    {
        if(ch == ',')
        {
            m_numCommas++;
            if(m_numCommas == 2)
            {
                m_state = stGrabStatusChar;
                m_pos = 0;
            }
        }
    }
    else if(m_state == stGrabStatusChar)
    {
        // expect active record only
        if(ch == 'A')
        {
            m_state = stWaitLatLonInfo;
        }
        else
        {
            // unexpected
            m_validLatLonPresent = false; // mark already catched data as no more valid
            m_state = stWaitLine;
            return;
        }
    }
    else if(m_state == stWaitLatLonInfo)
    {
        if(ch == ',')
        {
            // clear tmp buffer
            memset( m_tmpBuf, 0, MAX_TMP_BUFFER );
            m_pos = 0;
            m_numCommas = 0;
            m_state = stGrabLatLonInfo;
        }
        else
        {
            // should never happen
            m_state = stWaitLine;
            return;
        }
    }
    else if(m_state == stGrabLatLonInfo)
    {
        // fill m_tmpBuf by incoming chars

        m_tmpBuf[m_pos] = ch;
        m_pos++;
        if(m_pos >= MAX_TMP_BUFFER)
        {
            // should never happen
            m_state = stWaitLine;
            return;
        }

        if(ch == ',')
        {
            m_numCommas++;
        }

        if(m_numCommas == 4)
        {
            decodeLatLon();
            m_state = stWaitLine;
        }
    }

}
#endif

#ifdef USE_GNGLL
//---------------------------------------------------------------------------------
void GpsLatLonExtract::onCharReceived( char ch )
{
    for(size_t idx = 1; idx < 6; idx++)
    {
        m_rmc[idx - 1] = m_rmc[idx];
    }
    m_rmc[5] = ch;

    if(strncmp( m_rmc, "$GNGLL", 6 ) == 0)
    {
        m_state = stWaitLatLonInfo;
        m_numCommas = 0;
        return;
    }

    if(m_state == stWaitLatLonInfo)
    {
        if(ch == ',')
        {
            // clear tmp buffer
            memset( m_tmpBuf, 0, MAX_TMP_BUFFER );
            m_pos = 0;
            m_numCommas = 0;
            m_state = stGrabLatLonInfo;
        }
        else
        {
            // should never happen
            m_state = stWaitLine;
            return;
        }
    }
    else if(m_state == stWaitStatusChar)
    {
        if(ch == ',')
        {
            m_numCommas++;
            if(m_numCommas == 2)
            {
                m_state = stGrabStatusChar;
                m_pos = 0;
            }
        }
    }
    else if(m_state == stGrabStatusChar)
    {
        // expect active record only
        if(ch == 'A')
        {
            decodeLatLon();
            m_state = stWaitLine;
        }
        else
        {
            // unexpected
            m_validLatLonPresent = false; // mark already catched data as no more valid
            m_state = stWaitLine;
            return;
        }
    }
    else if(m_state == stGrabTime)
    {
        // skip all chars just wait comma
        if(ch == ',')
        {
            m_state = stGrabStatusChar;
        }
    }
    else if(m_state == stGrabLatLonInfo)
    {
        // fill m_tmpBuf by incoming chars

        m_tmpBuf[m_pos] = ch;
        m_pos++;
        if(m_pos >= MAX_TMP_BUFFER)
        {
            // should never happen
            m_state = stWaitLine;
            return;
        }

        if(ch == ',')
        {
            m_numCommas++;
        }

        if(m_numCommas == 4)
        {
            m_state = stGrabTime;
            return;
        }
    }
}
#endif


//---------------------------------------------------------------------------------
void GpsLatLonExtract::decodeLatLon()
{
    m_validLatLonPresent = false; // nonvalid so far
    m_state = stWaitLine;

    char latDegree[2];
    char latMinutesInt[2];
    char latMinutesFrac[MAX_FRAC_CHARS];
    char northORSouth;

    char lonDegree[3];
    char lonMinutesInt[2];
    char lonMinutesFrac[MAX_FRAC_CHARS];
    char westOREast;


    // Validate and catch. Buffer should look like "4916.45,N,12311.12,W"

    size_t pos = 0; // 
    ENSURE_DIGIT_AND_CATCH( latDegree[0] );
    ENSURE_DIGIT_AND_CATCH( latDegree[1] );
    ENSURE_DIGIT_AND_CATCH( latMinutesInt[0] );
    ENSURE_DIGIT_AND_CATCH( latMinutesInt[1] );
    ENSURE_POINT;
    memset( latMinutesFrac, '0', sizeof( latMinutesFrac ) );
    for(size_t idx = 0; idx < sizeof( latMinutesFrac ); idx++)
    {
        if(m_tmpBuf[pos] == ',')
            break;
        ENSURE_DIGIT_AND_CATCH( latMinutesFrac[idx] );
    }
    ENSURE_COMMA;
    ENSURE_EITHER_AND_CATCH( 'N', 'S', northORSouth );
    ENSURE_COMMA;
    ENSURE_DIGIT_AND_CATCH( lonDegree[0] );
    ENSURE_DIGIT_AND_CATCH( lonDegree[1] );
    ENSURE_DIGIT_AND_CATCH( lonDegree[2] );
    ENSURE_DIGIT_AND_CATCH( lonMinutesInt[0] );
    ENSURE_DIGIT_AND_CATCH( lonMinutesInt[1] );
    ENSURE_POINT;
    memset( lonMinutesFrac, '0', sizeof( lonMinutesFrac ) );
    for(size_t idx = 0; idx < sizeof( lonMinutesFrac ); idx++)
    {
        if(m_tmpBuf[pos] == ',')
            break;
        ENSURE_DIGIT_AND_CATCH( lonMinutesFrac[idx] );
    }
    ENSURE_COMMA;
    ENSURE_EITHER_AND_CATCH( 'W', 'E', westOREast );

    // copy to ready data
    memcpy( m_latDegree, latDegree, sizeof( m_latDegree ) );
    memcpy( m_latMinutesInt, latMinutesInt, sizeof( m_latMinutesInt ) );
    memcpy( m_latMinutesFrac, latMinutesFrac, sizeof( m_latMinutesFrac ) );
    m_northORSouth = northORSouth;

    memcpy( m_lonDegree, lonDegree, sizeof( m_lonDegree ) );
    memcpy( m_lonMinutesInt, lonMinutesInt, sizeof( m_lonMinutesInt ) );
    memcpy( m_lonMinutesFrac, lonMinutesFrac, sizeof( m_lonMinutesFrac ) );
    m_westOREast = westOREast;

    // mark as valid
    m_validLatLonPresent = true;
}


//---------------------------------------------------------------------------------
static void hlpConvertLocation( char* dst, unsigned degree, uint32_t secondsX100, bool isLongitude, bool isNegative )
{
    //
    // Convert degree+seconds (no minutes) into Maidenhead Locator. One coordinate at time.
    // Code utilizes integer arithmetic only, suitable for Arduino.
    // isLongitude=false - latitude
    // isLongitude=true - longitude).
    // isNegative=false - north or east
    // isNegative=true - south or west
    //
    // by RA9YER


    int actualDegree = 0;
    uint32_t actualSecondsX100 = secondsX100;
    const uint32_t secondsX100InOneDegree = 360000L;

    if( isLongitude )
    {
        actualDegree = (isNegative) ? 180 - degree : 180 + degree;
    }
    else
    {
        actualDegree = (isNegative) ? 90 - degree : 90 + degree;
    }

    if( isNegative )
    {
        if( secondsX100 != 0 )
        {
            actualDegree--;
            actualSecondsX100 = secondsX100InOneDegree - secondsX100;
        }
    }

    const unsigned N = (isLongitude)? 20 : 10;
    const unsigned K = (isLongitude)? 2 : 1;
    unsigned M = 15000 * K;

    // 10 or 20 degree / 18 parts
    unsigned v1 = actualDegree / N;

    // 1 or 2 degree / 10 parts
    unsigned v2 = (actualDegree - v1 * N) / K;
    unsigned v2_rest = actualDegree - v1 * N - v2 * K; // 0 or 1

    actualSecondsX100 += (v2_rest * secondsX100InOneDegree); // add rest from degree to seconds if present. 0 or 1 degree in secondsX100 units

    // fit into 24 parts
    unsigned v3 = actualSecondsX100 / M;  // 1 degree = 3600 seconds = 360000 in secondsX100 units,  fit into 24 parts -> M = 15000
    uint32_t v3_rest = actualSecondsX100 - (uint32_t)v3 * M;

    // fit into 10 parts
    M /= 10;
    unsigned v4 = v3_rest / M;
    uint32_t v4_rest = v3_rest - (uint32_t)v4 * M;

    // fit into 24 parts  (Note: this 5th pair maybe not very accurate)
    M /= 24;
    unsigned v5 = v4_rest / M;
    // uint32_t v5_rest = v4_rest - (uint32_t)v5 * M;

    dst[0] = 'A' + v1;
    dst[1] = '0' + v2;
    dst[2] = 'a' + v3;
    dst[3] = '0' + v4;
    dst[4] = 'a' + v5;

}

//---------------------------------------------------------------------------------
static unsigned myatoi( const char* ptr, size_t num )
{
    unsigned res = 0;
    unsigned m = 1;
    for( size_t idx = 0; idx < num; idx++ )
    {
        int v = ptr[num - idx - 1] - '0';
        res += v * m;
        m *= 10;
    }
    return res;
}

//---------------------------------------------------------------------------------
const char *GpsLatLonExtract::calcMaidenheadLocator( unsigned numChars )
{
    // static char g_resultBuf[11];

    size_t numPairs = numChars / 2;

    // correct input argument if need
    if( numPairs < 1 )
    {
        numPairs = 1; // like "AA"
    }
    else if( numPairs > 5 )
    {
        numPairs = 5; // like "AA00aa00aa"
    }

    // Note: We use only 3 most significant digits from seconds' frac part.
    // This is enough for 8 digits (4 pairs) locator and quite acceptable for 10 digits (5 parts) locator.

    // --- lat ---
    unsigned latDegree = myatoi( m_latDegree, sizeof( m_latDegree ) );
    uint32_t latSecondsX100 = (uint32_t)(myatoi( m_latMinutesInt, sizeof( m_latMinutesInt ) )) * 60 * 100 + (uint32_t)(myatoi( m_latMinutesFrac, 3 )) * 6;
    bool isLatNegative = (m_northORSouth == 'S');

    char latBuf[5];
    hlpConvertLocation( latBuf, latDegree, latSecondsX100, false, isLatNegative );

    // --- lon ---
    unsigned lonDegree = myatoi( m_lonDegree, sizeof( m_lonDegree ) );
    uint32_t lonSecondsX100 = (uint32_t)(myatoi( m_lonMinutesInt, sizeof( m_lonMinutesInt ) )) * 60 * 100 + (uint32_t)(myatoi( m_lonMinutesFrac, 3 )) * 6;
    bool isLonNegative = (m_westOREast == 'W');

    char lonBuf[5];
    hlpConvertLocation( lonBuf, lonDegree, lonSecondsX100, true, isLonNegative );


    // --- produce result ---
    size_t pos=0;
    for(size_t idx=0; idx<numPairs; idx++)
    {
        m_maidenheadLocator[pos++] = lonBuf[idx];
        m_maidenheadLocator[pos++] = latBuf[idx];
    }

    m_maidenheadLocator[pos] = 0; // null terminated char's string

    return m_maidenheadLocator;
}
