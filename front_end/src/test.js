var body = "";
// set to back in case they edit it
// set subject back in case they edit it

$('#reply').on('click', function(){
    if ($(this).hasClass('active')) {
        if ($('#forward').hasClass('active'))
            {
                $('#forward').removeClass('active');
                $('#forward').attr('aria-pressed', 'false');
                $('subject').val($('subject').val().replace('FWD: '), '');
            }

        $('label[for=time], input#time').hide();
        $('#to').attr('disabled', false);
        $('#to').val(from);
        $('#subject').val('RE: '+$('#subject').val());
        $('#subject').attr('disabled', false);
        
        $('label[for=from], input#from').hide();
        $('#body').attr('disabled', false);
        $('#body').text('');
        $('#body').addClass('mb-3');
        $('#oldBody').css('display', '');
        $('#emailForm').attr('action', '/api/:user/mbox/reply?uidl='+uidl);
    }
    else
    {
        $('label[for=time], input#time').show();
        $('#to').attr('disabled', true);
        $('#to').val(to);
        $('#subject').val(subject);
        $('#subject').attr('disabled', true);
        $('label[for=from], input#from').show();
        $('#body').attr('disabled', true);
        $('#body').text(body);
        $('#body').removeClass('mb-3');
        $('#oldBody').css('display', 'none');
    }
});


$('#forward').on('click', function(){
    if ($(this).hasClass('active')) {
        if ($('#reply').hasClass('active'))
        {
            $('#reply').removeClass('active');
            $('#reply').attr('aria-pressed', 'false');
            $('subject').val($('subject').val().replace('RE: '), '');
        }

        $('label[for=time], input#time').hide();
        $('#to').attr('disabled', false);
        $('#to').val('');
        $('#subject').val('FWD: '+$('#subject').val());
        $('#subject').attr('disabled', false);
        $('label[for=from], input#from').hide();
        $('#body').attr('disabled', false);
        $('#body').text('');
        $('#body').addClass('mb-3');
        $('#oldBody').css('display', '');
    }
    else
    {
        $('label[for=time], input#time').show();
        $('#to').attr('disabled', true);
        $('#to').val(to);
        $('#subject').val(subject);
        $('#subject').attr('disabled', true);
        $('label[for=from], input#from').show();
        $('#body').attr('disabled', true);
        $('#body').text(body);
        $('#body').removeClass('mb-3');
        $('#oldBody').css('display', 'none');
    }
});