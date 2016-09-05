var guardianAPIkey = '8f539d82-e75a-405a-8c91-63dd323de321';
var guardianSection = 'commentisfree%7Cbusiness%7Cnews'; // 'business|commentisfree|edinburgh|environment|news';
var sectionList = ['theguardian', 'commentisfree', 'business', 'environment', 'politics', 'society', 'culture', 'weather', 'world', 'local-government-network'];
var guardianData;
var REQUEST_TYPE_HEADLINE = 1;
var REQUEST_TYPE_STORY = 2;
var REQUEST_TYPE_SECTION = 3;
var displayBufferSize = 1000 - 25;
var story;
var currentStoryID;

function getGuardianData() {
  var url = 'http://content.guardianapis.com/search?api-key=' + guardianAPIkey + '&section=' + guardianSection;
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    guardianData = JSON.parse(this.responseText);
    currentStoryID = -1;
    sendHeadline(0);
  };
  xhr.open('GET', url);
  xhr.send();  
}


function sendSection(itemNo) {
  if( itemNo < sectionList.length ) {
    var dictionary = {
      'KEY_ITEM_NO': itemNo,
      'KEY_ITEM_HEAD' : sectionList[itemNo]
    };
    Pebble.sendAppMessage(dictionary,
                         function(e) {
                           sendSection(itemNo+1);
                         });
  }
}

function sendHeadline(itemNo) {
  if( itemNo < guardianData.response.results.length ) {
  // Assemble dictionary using our keys
    var dictionary = {
      'KEY_ITEM_NO': itemNo,
      'KEY_ITEM_HEAD': guardianData.response.results[itemNo].webTitle
    };

    // Send to Pebble
    Pebble.sendAppMessage(dictionary,
      function(e) {
        console.log('Headline ' + itemNo + ' sent to Pebble successfully!');
        sendHeadline(itemNo+1);
      },
        function(e) {
        console.log('Error sending headline ' + itemNo + ' to Pebble!');
      }
    );
  } else {
    console.log('Tried to send item ' + itemNo + ' out of bounds');
  }
}

function getStory(itemNo) {
  if( itemNo < guardianData.response.results.length ) {
    var url = guardianData.response.results[itemNo].apiUrl;
    url += "?api-key=" + guardianAPIkey;
    url += "&show-fields=body";
    currentStoryID = 100 - itemNo;
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
      var storyData = JSON.parse(this.responseText);
      story = storyData.response.content.webTitle;
      story += '\n';
      story += storyData.response.content.webPublicationDate.replace(/T[^Z]*Z/, '\n\n');
      story += storyData.response.content.fields.body;
      story = story.replace(/<\/p>\s*/g, '\n\n');
      story = story.replace(/<[^>]*>/g, '');
      story = story.replace(/( |\n)( +)/g, '$1');
      var entities = [
        ['apos', '\''],
        ['amp', '&'],
        ['lt', '<'],
        ['gt', '>'],
        ['#39', "'"],
        ['mdash', '-'],
        ['quot', '\"']
      ];

      for (var i = 0, max = entities.length; i < max; ++i) 
        story = story.replace(new RegExp('&'+entities[i][0]+';', 'g'), entities[i][1]);
      currentStoryID = 100 - currentStoryID;
      sendStory(currentStoryID, 0);
    };
    xhr.open('GET', url);
    xhr.send();
  }
}

function sendStory(itemNo, pageNo) {
  if(itemNo == currentStoryID) {
    var dictionary = {
      'KEY_ITEM_TEXT' : story.substring(displayBufferSize * pageNo, displayBufferSize * (pageNo + 1))
    };
    Pebble.sendAppMessage(dictionary,
                         function(e) {
                           console.log('Sent story\n' + story.substring(displayBufferSize*pageNo, displayBufferSize*(pageNo+1)));
                         },
                         function(e) {
                           console.log('Story send failed\n' + story.substring(displayBufferSize*pageNo, displayBufferSize*(pageNo+1)));
                         });
  } else {
    getStory(itemNo);
  }
}


// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log('GPebbleKit JS ready!');
   // getGuardianData();
    sendSection(0);
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log('GAppMessage received!' + JSON.stringify(e.payload));
    var requestedItem = e.payload.KEY_ITEM_NO;
    var requestType = e.payload.KEY_REQUEST_TYPE;
    var requestedPage = e.payload.KEY_ITEM_PAGE;
    console.log('Requested item is ' + requestedItem);
    if( requestedItem >= 0 && requestedItem < 10 ) {
      console.log('Trying to send item ' + requestedItem);
      switch( requestType ) {
        case REQUEST_TYPE_HEADLINE:
          sendHeadline(requestedItem);
          break;
        case REQUEST_TYPE_STORY:
          sendStory(requestedItem, requestedPage);
          break;
        case REQUEST_TYPE_SECTION:
          guardianSection = sectionList[requestedItem];
          getGuardianData();
          break;
      } 
    }
    else {
      Pebble.sendAppMessage({'KEY_ITEM_NO':-1},
        function(e) {
          console.log('End signal sent to Pebble successfully!');
        },
        function(e) {
          console.log('Error sending end signal to Pebble!');
        }
      );
    }
  }                     
);
